// SPDX-License-Identifier: MIT
#include "Manager.hpp"
#include "Fw/Time/TimeInterval.hpp"
#include "Os/Task.hpp"
namespace Ina229 {
namespace {
Configuration toCore(const DeviceConfig &config) {
    Configuration c;
    c.shuntOhms = config.get_shuntOhms();
    c.maxCurrent = config.get_maxCurrent();
    c.narrowRange = config.get_narrowRange();
    c.temperatureCompensation = config.get_temperatureCompensation();
    c.conversionDelaySteps = config.get_conversionDelaySteps();
    c.shuntTempco = config.get_shuntTempco();
    c.adc.mode = config.get_mode();
    c.adc.busTime = config.get_busTime();
    c.adc.shuntTime = config.get_shuntTime();
    c.adc.temperatureTime = config.get_temperatureTime();
    c.adc.averaging = config.get_averaging();
    return c;
}
} // namespace
Manager::Manager(const char *name) : ManagerComponentBase(name), m_bus(*this), m_device(m_bus, m_delay) {}
bool Manager::PortBus::transfer(const std::uint8_t *tx, std::uint8_t *rx, std::size_t size) {
    if (!tx || !rx || size == 0 || size > 6 || !m_owner.isConnected_spi_OutputPort(0))
        return false;
    Fw::Buffer writeBuffer(const_cast<U8 *>(tx), static_cast<FwSizeType>(size));
    Fw::Buffer readBuffer(rx, static_cast<FwSizeType>(size));
    const auto status = m_owner.spi_out(0, writeBuffer, readBuffer);
    return status == Drv::SpiStatus::SPI_OK && readBuffer.getSize() == size && writeBuffer.getSize() == size &&
           readBuffer.getData() == rx && writeBuffer.getData() == tx;
}
bool Manager::OsDelay::milliseconds(std::uint32_t duration) {
    return Os::Task::delay(Fw::TimeInterval(duration / 1000, (duration % 1000) * 1000)) == Os::Task::OP_OK;
}
DriverStatus Manager::convert(Status status) {
    switch (status) {
    case Status::OK:
        return DriverStatus::OK;
    case Status::INVALID_ARGUMENT:
        return DriverStatus::INVALID_ARGUMENT;
    case Status::INVALID_STATE:
        return DriverStatus::INVALID_STATE;
    case Status::SPI_ERROR:
        return DriverStatus::SPI_ERROR;
    case Status::IDENTITY_MISMATCH:
        return DriverStatus::IDENTITY_MISMATCH;
    case Status::VERIFY_ERROR:
        return DriverStatus::VERIFY_ERROR;
    case Status::NOT_READY:
        return DriverStatus::NOT_READY;
    case Status::DEVICE_ERROR:
        return DriverStatus::DEVICE_ERROR;
    case Status::DELAY_ERROR:
        return DriverStatus::DELAY_ERROR;
    default:
        return DriverStatus::INVALID_STATE;
    }
}
void Manager::clearThrottles() {
    log_WARNING_HI_OperationFailed_ThrottleClear();
    log_ACTIVITY_LO_Recovered_ThrottleClear();
    log_WARNING_HI_MeasurementDegraded_ThrottleClear();
    log_ACTIVITY_LO_MeasurementRestored_ThrottleClear();
}
void Manager::record(const Operation &operation, Status status) {
    // Polling before conversion completes is routine, not a fault or recovery.
    if (status == Status::NOT_READY)
        return;
    if (status != Status::OK)
        ++m_errors;
    // Edges only; FPP throttles bound the rate when a fault is intermittent.
    if (status != m_last) {
        tlmWrite_LastStatus(convert(status));
        if (status != Status::OK)
            log_WARNING_HI_OperationFailed(operation, convert(status));
        else
            log_ACTIVITY_LO_Recovered(operation);
    }
    m_last = status;
}
void Manager::recordValidity(U8 valid, U8 expected) {
    if (valid != expected) {
        if (!m_degraded || valid != m_degradedMask)
            log_WARNING_HI_MeasurementDegraded(valid, expected);
        m_degraded = true;
        m_degradedMask = valid;
    } else if (m_degraded) {
        log_ACTIVITY_LO_MeasurementRestored(valid);
        m_degraded = false;
    }
}
void Manager::respond(FwOpcodeType opcode, U32 sequence, const Operation &operation, Status status) {
    // A rejected ground argument or an out-of-sequence command is not a device fault: the command
    // response and the command dispatcher's opcode-error event are the operator evidence.
    if (status != Status::INVALID_ARGUMENT && status != Status::INVALID_STATE)
        record(operation, status);
    const auto response = status == Status::OK                 ? Fw::CmdResponse::OK
                          : status == Status::INVALID_ARGUMENT ? Fw::CmdResponse::VALIDATION_ERROR
                                                               : Fw::CmdResponse::EXECUTION_ERROR;
    cmdResponse_out(opcode, sequence, response);
}
Status Manager::initialize(const DeviceConfig &config) {
    const auto status = m_device.initialize(toCore(config));
    if (status == Status::OK) {
        m_degraded = false;
        clearThrottles();
    }
    return status;
}
Status Manager::acquire(Reading &sample) {
    Measurement m;
    const auto status = m_device.read(m);
    if (status == Status::OK) {
        sample.set_busVolts(m.busVolts);
        sample.set_shuntVolts(m.shuntVolts);
        sample.set_temperatureC(m.temperatureC);
        sample.set_currentAmps(m.currentAmps);
        sample.set_powerWatts(m.powerWatts);
        sample.set_energyJoules(m.energyJoules);
        sample.set_chargeCoulombs(m.chargeCoulombs);
        sample.set_rawShunt(m.rawShunt);
        sample.set_rawBus(m.rawBus);
        sample.set_rawTemperature(m.rawTemperature);
        sample.set_rawCurrent(m.rawCurrent);
        sample.set_rawPower(m.rawPower);
        sample.set_rawEnergy(m.rawEnergy);
        sample.set_rawCharge(m.rawCharge);
        sample.set_diagnostics(m.diagnostics);
        sample.set_validMask(m.validMask);
        ++m_samples;
        recordValidity(m.validMask, m_device.expectedValidMask());
    }
    record(Operation::READ, status);
    return status;
}
DriverStatus Manager::configure_handler(FwIndexType, const DeviceConfig &config) {
    const auto status = initialize(config);
    record(Operation::INITIALIZE, status);
    return convert(status);
}
DriverStatus Manager::read_handler(FwIndexType, Reading &sample) {
    return convert(acquire(sample));
}
void Manager::INITIALIZE_cmdHandler(FwOpcodeType op, U32 seq, const DeviceConfig &config) {
    respond(op, seq, Operation::INITIALIZE, initialize(config));
}
void Manager::CONFIGURE_ADC_cmdHandler(FwOpcodeType op, U32 seq, U8 mode, U8 busTime, U8 shuntTime,
                                       U8 temperatureTime, U8 averaging) {
    AdcConfiguration adc;
    adc.mode = mode;
    adc.busTime = busTime;
    adc.shuntTime = shuntTime;
    adc.temperatureTime = temperatureTime;
    adc.averaging = averaging;
    respond(op, seq, Operation::CONFIGURE_ADC, m_device.configureAdc(adc));
}
void Manager::TRIGGER_cmdHandler(FwOpcodeType op, U32 seq) {
    respond(op, seq, Operation::TRIGGER, m_device.trigger());
}
void Manager::STOP_cmdHandler(FwOpcodeType op, U32 seq) {
    respond(op, seq, Operation::STOP, m_device.stop());
}
void Manager::RESET_ACCUMULATORS_cmdHandler(FwOpcodeType op, U32 seq) {
    respond(op, seq, Operation::RESET_ACCUMULATORS, m_device.resetAccumulators());
}
void Manager::SET_ALERT_CONFIG_cmdHandler(FwOpcodeType op, U32 seq, U16 bits) {
    respond(op, seq, Operation::SET_ALERT_CONFIG, m_device.setAlertConfig(bits));
}
void Manager::SET_THRESHOLD_cmdHandler(FwOpcodeType op, U32 seq, U8 address, U16 raw) {
    respond(op, seq, Operation::SET_THRESHOLD, m_device.setThreshold(static_cast<Register>(address), raw));
}
void Manager::READ_SNAPSHOT_cmdHandler(FwOpcodeType op, U32 seq) {
    Reading sample;
    const auto status = acquire(sample);
    if (status == Status::OK)
        tlmWrite_Measurement(sample);
    cmdResponse_out(op, seq, status == Status::OK ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}
void Manager::REPORT_cmdHandler(FwOpcodeType op, U32 seq) {
    tlmWrite_Initialized(m_device.isInitialized() ? 1 : 0);
    tlmWrite_LastStatus(convert(m_last));
    tlmWrite_Samples(m_samples);
    tlmWrite_Errors(m_errors);
    cmdResponse_out(op, seq, Fw::CmdResponse::OK);
}
void Manager::CLEAR_EVENT_THROTTLE_cmdHandler(FwOpcodeType op, U32 seq) {
    clearThrottles();
    cmdResponse_out(op, seq, Fw::CmdResponse::OK);
}
} // namespace Ina229
