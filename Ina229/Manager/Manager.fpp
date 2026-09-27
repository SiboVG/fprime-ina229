module Ina229 {
  @ One sensor manager. Every command and input port is guarded by the component mutex, so
  @ command-dispatcher and scheduler threads are serialized per instance.
  passive component Manager {
    guarded input port configure: Ina229.Configure
    guarded input port read: Ina229.Read
    @ Standard status-returning F Prime SPI port. SPI_OK means the whole transfer completed
    @ under one chip-select assertion with both buffers unchanged.
    output port spi: Drv.SpiWriteRead
    guarded command INITIALIZE(config: Ina229.DeviceConfig)
    guarded command CONFIGURE_ADC(mode: U8, busTime: U8, shuntTime: U8, temperatureTime: U8, averaging: U8)
    guarded command TRIGGER
    guarded command STOP
    guarded command RESET_ACCUMULATORS
    guarded command SET_ALERT_CONFIG(bits: U16)
    guarded command SET_THRESHOLD(address: U8, raw: U16)
    guarded command READ_SNAPSHOT
    guarded command REPORT
    @ Re-enable throttled events; a successful INITIALIZE also clears them.
    guarded command CLEAR_EVENT_THROTTLE
    event OperationFailed(operation: Ina229.Operation, status: Ina229.DriverStatus) \
      severity warning high format "INA229 {} failed: {}" throttle 10
    event Recovered(operation: Ina229.Operation) \
      severity activity low format "INA229 {} recovered" throttle 10
    event MeasurementDegraded(validMask: U8, expectedMask: U8) \
      severity warning high format "INA229 valid channels 0x{x} below expected 0x{x}" throttle 10
    event MeasurementRestored(validMask: U8) \
      severity activity low format "INA229 valid channels restored to 0x{x}" throttle 10
    telemetry Initialized: U8
    telemetry LastStatus: Ina229.DriverStatus
    telemetry Samples: U64
    telemetry Errors: U64
    telemetry Measurement: Ina229.Reading
    time get port timeCaller
    command reg port cmdRegOut
    command recv port cmdIn
    command resp port cmdResponseOut
    text event port logTextOut
    event port logOut
    telemetry port tlmOut
  }
}
