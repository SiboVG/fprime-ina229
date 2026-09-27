# F Prime integration

The tested API is F Prime `35d4c0e` (v4.2.0-267). Activate that checkout's Python tool chain before
CMake generation.

## F Prime tool chain

Component tests and deployments need the Python tools pinned by the F Prime checkout at
the tested revision, `35d4c0e881482adf372b8eee3eb56ad2dee74618` (v4.2.0-267), the same revision fprime-csp-libparam and the
TE0715 Zephyr workspace use. In the CubeSpec workspace, create that environment with:

```sh
tools/workspace/bootstrap-fprime-venv.sh --fprime /absolute/path/to/fprime --venv .venv-fprime
. .venv-fprime/bin/activate
```

Outside the workspace, the equivalent is:

```sh
python3 -m venv .venv-fprime && . .venv-fprime/bin/activate
python -m pip install -r /absolute/path/to/fprime/requirements.txt
# macOS only: the pinned CMake 3.26.0 fails inside F Prime's build on macOS 27,
# so remove it and use a newer host CMake (for example from Homebrew).
python -m pip uninstall -y cmake
```

This revision pins fpp 3.3.0a12, which has native builds for Linux (x86_64 and aarch64)
and macOS (arm64 and x86_64), so every host runs the same fpp release natively. The
bootstrap script also handles older F Prime checkouts whose fpp has no native build for
the host, by installing that release's platform-independent (JVM) wheels. If a macOS
link fails with `tapi error: malformed file`, the selected Xcode is older than the
default SDK: update Xcode, or select the matching Command Line Tools with
`sudo xcode-select -s /Library/Developer/CommandLineTools`.

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
                                 Manager.spi: Drv.SpiWriteRead
                                   -> platform generic SPI driver
                                       -> hardware SPI / one chip select
```

The manager's `spi` output is F Prime's standard `Drv.SpiWriteRead(ref writeBuffer,
ref readBuffer) -> Drv.SpiStatus`. The driver must return `SPI_OK` only after the entire
transfer completes under one chip-select assertion, and must not retain or replace the
buffer pointers or change their sizes. The manager treats any other status, or a changed
buffer, as a failed transfer. `Drv.LinuxSpiDriver`, fprime-zephyr's `ZephyrSpiDriver`,
and the example's Unix bridge all implement this port, so sensor code does not change
between platforms. The deprecated void `Drv.SpiReadWrite` port cannot report failures and
is not used.

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
