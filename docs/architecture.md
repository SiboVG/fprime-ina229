# Architecture and ownership

The application owns **when and why** to acquire. The manager owns one sensor's
F Prime interface and operational reporting. The device core owns INA229 behavior.
The platform driver owns **how** a bounded SPI transaction reaches hardware.

```text
Mission application / scheduler / fault policy
             | Configure and Read ports
      Ina229::Manager             commands, events, telemetry
             | owns
      Ina229::Device              register protocol and SI values
             | SpiBus::transfer + Delay::milliseconds
      manager port adapter        synchronous status-bearing SpiTransfer
             |
      platform SPI driver         controller, bus arbitration, chip select, deadline
             |
           INA229
```

The injected core can be used without F Prime:

```cpp
// NativeSpi implements Ina229::SpiBus; NativeDelay implements Ina229::Delay.
NativeSpi spi;
NativeDelay delay;
Ina229::Device sensor(spi, delay);
Ina229::Configuration config;
config.shuntOhms = 0.01;
config.maxCurrent = 8.0;
const auto status = sensor.initialize(config);
if (status == Ina229::Status::OK) {
    Ina229::Measurement sample;
    // Poll later when acquisition is due; NOT_READY is not a valid sample.
    if (sensor.read(sample) == Ina229::Status::OK) {
        consume(sample); // application-defined
    }
}
```

In F Prime, the manager's adapter connects that same core to a generic byte-level
port. A native driver must report transfer failure; a void SPI port cannot provide
this guarantee on its own. A custom platform adapter may use a status-returning
HAL/OS API internally. Implement one component instance per chip-select or carry
bus/CS selection in the platform component configuration, outside INA229 registers.

No core function allocates memory. Scratch buffers are at most six bytes; acquisition
uses a fixed seven-element register array. No transport is called from construction.
The core retains references to its transport/delay providers, never to caller sample
or transfer buffers. A read commits its output only after all transfers succeed.

The passive manager is intentionally single-owner and contains no mutex. Route its
commands and read/configure calls through one serialized application context. An
active application component can serialize command dispatch and scheduler input.
A shared physical SPI driver must separately arbitrate other clients and enforce
its deadline. Don't invoke blocking initialization or SPI from an interrupt.

The library does not impose deployment rates, filtering, data products, current-trip
actions, reset/retry policy, or a power budget. Those policies differ between a bench
and flight software. A mission can connect the same manager to a native controller
while the [example](ft232h.md) uses a generic Unix-to-PyFtdi byte bridge.
