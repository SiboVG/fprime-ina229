# F Prime integration

The tested API is F Prime 3.5.1. Activate that checkout's Python tool chain before
CMake generation.

## F Prime tool chain

Component tests and deployments need the Python tools pinned by the F Prime 3.5.1
checkout, including fpp 2.2.1. In the CubeSpec workspace, create that environment with:

```sh
tools/workspace/bootstrap-fprime-venv.sh --fprime /absolute/path/to/fprime --venv .venv-fprime
. .venv-fprime/bin/activate
```

Outside the workspace, the equivalent is:

```sh
python3 -m venv .venv-fprime && . .venv-fprime/bin/activate
python -m pip install -r /absolute/path/to/fprime/requirements.txt
# macOS only: fpp's macOS wheels contain x86_64 executables, so install the same
# release's platform-independent (JVM) wheels instead. Requires Java 17 or newer.
python -m pip download --no-deps --only-binary=:all: --platform any -d fpp-wheels \
  $(grep -o '^fprime-fpp-[a-z-]*==[^ ;#]*' /absolute/path/to/fprime/requirements.txt)
python -m pip install --force-reinstall --no-deps fpp-wheels/*.whl
```

Linux hosts use fpp's native wheels. The macOS JVM wheels run the same fpp release, so
every host generates identical code. If a macOS link fails with `tapi error: malformed
file`, the selected Xcode is older than the default SDK: update Xcode, or select the
matching Command Line Tools with `sudo xcode-select -s /Library/Developer/CommandLineTools`.

## Consumer registration

Add the library to the consumer's `settings.ini`:

```ini
[fprime]
framework_path: ./fprime
project_root: .
library_locations: /absolute/path/to/fprime-ina229
```

Targets: `Ina229_Device` (portable core), `Ina229_Types` (serializable data and
ports), `Ina229_Manager` (passive device manager). `library.cmake` registers only
these modules; examples and their dependencies are never pulled into flight code.

```text
Application / scheduler / fault policy
  configure: Ina229.Configure -> Manager.configure
  read: Ina229.Read           -> Manager.read
                                 Manager.spi: Ina229.SpiTransfer
                                   -> platform generic SPI driver
                                       -> hardware SPI / one chip select
```

`SpiTransfer(ref Fw.Buffer tx, ref Fw.Buffer rx) -> TransferStatus` is a generic
full-duplex transaction contract. Return OK only after the entire transfer
completes under one CS assertion. Drivers must not retain or replace pointers
or change buffer sizes. A short transfer must return ERROR. The manager checks
status, sizes and pointer identity. Use a native controller driver on the flight
target; FT232H and the example's Unix bridge can be replaced without changing
sensor code. A void-returning `Drv.SpiReadWrite` port cannot satisfy this error
contract by simply returning OK after invocation.

`Configure(config: DeviceConfig)` takes its argument by value: the manager copies the
configuration and never writes to the caller's object. `Read(ref sample: Reading)`
writes the sample only on success.

## Execution context

Every command and input port is `guarded`, so the command dispatcher and the
application's scheduler task are serialized by the component mutex of each manager
instance. See [architecture](architecture.md#execution-context) for the per-operation
hold times. The worst case is `INITIALIZE`: a 2 ms delay plus 13 transfers. The mutex
does not arbitrate a shared SPI driver, and the manager starts no tasks. Schedule every
call outside interrupt context. Each instance owns one core device; provide stable
task and driver lifetimes.

FPP DeviceConfig mirrors the checked core configuration. Mode/time/averaging are
register field encodings, validated before I/O rather than silently truncated.

## Commands

| Command | Behavior |
| --- | --- |
| `INITIALIZE(config)` | Reset and configure; on success clears event throttles |
| `CONFIGURE_ADC`, `TRIGGER`, `STOP` | ADC mode and conversion control |
| `RESET_ACCUMULATORS` | Zero ENERGY/CHARGE and clear the accumulator-invalid latch |
| `SET_ALERT_CONFIG(bits)` | DIAG_ALRT bits 15..12 only; preserves MEMSTAT |
| `SET_THRESHOLD(address, raw)` | Raw alarm limit, registers 0x0C..0x11 |
| `READ_SNAPSHOT` | One read; publishes `Measurement` telemetry on success |
| `REPORT` | Publishes initialization state, last status and counters |
| `CLEAR_EVENT_THROTTLE` | Re-enables all throttled events |

Every command emits exactly one response. Invalid arguments map to VALIDATION_ERROR,
and other failures, including NOT_READY, map to EXECUTION_ERROR. A rejected argument
or an out-of-sequence command (INVALID_STATE, such as TRIGGER in a continuous mode) is
operator input, not a device fault. It neither counts in `Errors` nor emits
`OperationFailed`: the command response and the dispatcher's opcode-error event are the
evidence.

## Events and telemetry

| Event | Severity | When | Throttle |
| --- | --- | --- | --- |
| `OperationFailed(operation, status)` | WARNING_HI | Status changes to a failure | 10 |
| `Recovered(operation)` | ACTIVITY_LO | Status changes back to OK | 10 |
| `MeasurementDegraded(validMask, expectedMask)` | WARNING_HI | A successful read reports fewer channels than the mode produces, or a different reduced set | 10 |
| `MeasurementRestored(validMask)` | ACTIVITY_LO | All expected channels are valid again | 10 |

Events fire on edges, never per read, and the FPP throttle bounds the total if a fault
is intermittent. Throttles clear on `CLEAR_EVENT_THROTTLE` or a successful
initialization. `Errors` counts every failed operation, including those whose events are
throttled, so `REPORT` still shows the true rate. NOT_READY is routine polling and changes
neither the fault count nor the last-status edge.

The read port returns a Reading only on success and never publishes bulk sample
telemetry automatically. READ_SNAPSHOT publishes one Measurement telemetry item.
The frame values include SI units, raw data, DIAG_ALRT and a channel-valid mask.
The application should supply its own acquisition timestamp; sequential continuous
register reads must not be described as a simultaneous electrical sample.
Connect the normal command, event, time and telemetry patterns in a mission deployment.
Configure nonoverlapping base IDs.

Run the generated command/port/event/telemetry tests:

```sh
cmake -S tests/fprime-consumer -B /tmp/ina229-components -G Ninja \
  -DFPRIME_FRAMEWORK_PATH=/absolute/path/to/fprime
cmake --build /tmp/ina229-components --target Ina229_Manager_ut_exe
/tmp/ina229-components/bin/$(uname)/Ina229_Manager_ut_exe
```
