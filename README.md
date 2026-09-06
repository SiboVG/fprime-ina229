# F Prime INA229

Reusable INA229 voltage, current, power, energy, charge and temperature monitoring.
`Ina229::Device` is a fixed-capacity C++11 core using injected synchronous SPI and
delay interfaces. It does not depend on FT232H, Python, F Prime, or an OS.

The F Prime manager and optional FT232H bench are separate layers. The application
owns sample scheduling, shunt selection, calibration policy, alarm limits, and
fault response. SPI success must mean a completed full-length transfer.

Run portable tests:

```sh
cmake -S tests -B /tmp/ina229-tests -G Ninja
cmake --build /tmp/ina229-tests
ctest --test-dir /tmp/ina229-tests --output-on-failure
```

This is candidate integration software, not flight-qualified software. See
[device API and limits](docs/device.md) and [provenance](docs/provenance.md).

See [F Prime integration](docs/fprime.md) for typed ports, commands, telemetry,
execution context and generated component tests.

The optional [FT232H deployment](docs/ft232h.md) connects a generated F Prime
topology to PyFtdi through a generic byte bridge. See its wiring and run commands.
