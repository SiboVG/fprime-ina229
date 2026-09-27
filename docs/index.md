# F Prime INA229

A reusable C++11 driver for the TI INA229 voltage/current/power monitor with an
F Prime manager (tested with F Prime `35d4c0e`, v4.2.0-267) and an optional FT232H
bench deployment.

| Layer | Responsibility | Dependencies |
| --- | --- | --- |
| Device | Register protocol, checked calibration, units, validity, lifecycle | C++11, injected SPI and delay interfaces |
| Manager | Typed F Prime ports, commands, events and telemetry | Device, F Prime |
| Platform driver | One full-duplex SPI exchange with status and timeout | Chosen SPI controller and OS |
| Application | Sampling, timestamps, shunt/calibration policy, alarms and fault response | Manager ports |
| FT232H example | Host scheduling and USB SPI access | POSIX socket, optional PyFtdi |

See [architecture](architecture.md), [device API](device.md),
[F Prime integration](fprime.md), and the [FT232H deployment](ft232h.md).

Run portable C++ tests from the repository root:

```sh
cmake -S tests -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host --output-on-failure
python3 -m unittest discover -s tests -p test_bridge.py
```

The normal F Prime library registration includes no FT232H or Python dependencies.
For a native SPI platform, supply a driver that implements the status-returning `Drv.SpiWriteRead` port; the
device and manager sources remain the same.

This is candidate integration software. [Verification](verification.md) separates
executed host/component tests from outstanding physical and target validation.
