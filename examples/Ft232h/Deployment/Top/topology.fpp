module Ina229Bench {
  instance sensor: Ina229.Manager base id 0x1000
  instance spi: Ina229Bench.BridgeDriver base id 0x2000
  topology Bench {
    instance sensor
    instance spi
    connections SensorBus { sensor.spi -> spi.exchange }
  }
}
