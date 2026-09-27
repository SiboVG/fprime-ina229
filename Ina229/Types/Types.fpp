module Ina229 {
  enum DriverStatus {
    OK
    INVALID_ARGUMENT
    INVALID_STATE
    SPI_ERROR
    IDENTITY_MISMATCH
    VERIFY_ERROR
    NOT_READY
    DEVICE_ERROR
    DELAY_ERROR
  }
  @ Manager operation named in operator events.
  enum Operation {
    INITIALIZE
    CONFIGURE_ADC
    TRIGGER
    STOP
    RESET_ACCUMULATORS
    SET_ALERT_CONFIG
    SET_THRESHOLD
    READ
  }
  @ Completion of a generic synchronous SPI exchange.
  enum TransferStatus { OK, ERROR }
  @ Buffers are caller-owned and valid only for this synchronous call.
  @ ERROR includes incomplete transfers. Successful drivers leave sizes unchanged.
  port SpiTransfer(ref tx: Fw.Buffer, ref rx: Fw.Buffer) -> TransferStatus
  struct DeviceConfig {
    shuntOhms: F64
    maxCurrent: F64
    narrowRange: bool
    temperatureCompensation: bool
    conversionDelaySteps: U8
    shuntTempco: U16
    mode: U8
    busTime: U8
    shuntTime: U8
    temperatureTime: U8
    averaging: U8
  }
  @ validMask bits: 0 bus, 1 shunt, 2 temperature, 3 current, 4 power, 5 energy, 6 charge.
  struct Reading {
    busVolts: F64
    shuntVolts: F64
    temperatureC: F64
    currentAmps: F64
    powerWatts: F64
    energyJoules: F64
    chargeCoulombs: F64
    rawShunt: I32
    rawBus: U32
    rawTemperature: I16
    rawCurrent: I32
    rawPower: U32
    rawEnergy: U64
    rawCharge: I64
    diagnostics: U16
    validMask: U8
  }
  @ Input-only: the manager copies the configuration and never modifies the caller's value.
  port Configure(config: DeviceConfig) -> DriverStatus
  @ Returns NOT_READY without waiting; leaves sample unchanged on failure.
  port Read(ref sample: Reading) -> DriverStatus
}
