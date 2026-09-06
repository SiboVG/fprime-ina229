# F Prime integration

The tested API is F Prime 3.5.1. Activate that checkout's Python toolchain before
CMake generation. Add the library to the consumer's `settings.ini`:

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

The application serializes commands and reads on one task, owns the bus/CS,
sets deadlines, and interprets measurement validity. The passive manager neither
starts tasks nor arbitrates shared SPI hardware. Initialization waits 2 ms and
all I/O calls block for the driver timeout; schedule them outside interrupt
context. Each instance owns one core device. Provide stable task/driver lifetimes.

FPP DeviceConfig mirrors the checked core configuration. Mode/time/averaging are
register field encodings, validated before I/O rather than silently truncated.
The manager provides INITIALIZE, CONFIGURE_ADC, TRIGGER, STOP,
RESET_ACCUMULATORS, SET_ALERT_CONFIG, SET_THRESHOLD, READ_SNAPSHOT and REPORT
commands. Every command emits exactly one response; invalid arguments map to
VALIDATION_ERROR, other failures including NOT_READY to EXECUTION_ERROR.

The read port returns a Reading only on success and never publishes bulk sample
telemetry automatically. READ_SNAPSHOT publishes one Measurement telemetry item.
REPORT snapshots current initialization state and counters. Equal repeated
failures increment an internal error counter but emit a warning only on a status
edge; OK emits recovery. NOT_READY is routine polling and changes neither fault
count nor the last fault/recovery state. Connect the normal command, event, time
and telemetry patterns in a mission deployment. Configure nonoverlapping base IDs.

The frame values include SI units, raw data, DIAG_ALRT and a channel-valid mask.
The application should supply its own acquisition timestamp; sequential continuous
register reads must not be described as a simultaneous electrical sample.

Run the generated command/port/event/telemetry tests:

```sh
cmake -S tests/fprime-consumer -B /tmp/ina229-components -G Ninja \
  -DFPRIME_FRAMEWORK_PATH=/absolute/path/to/fprime
cmake --build /tmp/ina229-components --target Ina229_Manager_ut_exe
/tmp/ina229-components/bin/$(uname)/Ina229_Manager_ut_exe
```
