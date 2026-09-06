module Ina229 {
  @ One sensor manager. The invoking deployment must serialize all access.
  passive component Manager {
    sync input port configure: Ina229.Configure
    sync input port read: Ina229.Read
    output port spi: Ina229.SpiTransfer
    sync command INITIALIZE(config: Ina229.DeviceConfig)
    sync command CONFIGURE_ADC(mode: U8, busTime: U8, shuntTime: U8, temperatureTime: U8, averaging: U8)
    sync command TRIGGER
    sync command STOP
    sync command RESET_ACCUMULATORS
    sync command SET_ALERT_CONFIG(bits: U16)
    sync command SET_THRESHOLD(address: U8, raw: U16)
    sync command READ_SNAPSHOT
    sync command REPORT
    event OperationFailed(status: Ina229.DriverStatus) severity warning high format "INA229 operation failed: {}"
    event Recovered severity activity low format "INA229 operation recovered"
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
