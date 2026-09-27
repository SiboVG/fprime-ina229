module Ina229Bench {
  instance sensor: Ina229.Manager base id 0x1000
  instance spi: Ina229Bench.BridgeDriver base id 0x2000
  instance client: Ina229Bench.BenchClient base id 0x3000
  topology Bench {
    instance sensor
    instance spi
    instance client
    connections SensorBus { sensor.spi -> spi.exchange }
    connections Acquisition {
      client.configure -> sensor.configure
      client.read -> sensor.read
    }
  }
}
