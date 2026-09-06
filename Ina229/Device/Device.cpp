// SPDX-License-Identifier: MIT
#include "Device.hpp"
#include <cmath>
namespace Ina229 {
namespace {
std::size_t width(Register reg) {
    switch (reg) {
    case Register::ENERGY:
    case Register::CHARGE:
        return 5;
    case Register::VSHUNT:
    case Register::VBUS:
    case Register::CURRENT:
    case Register::POWER:
        return 3;
    default: {
        const auto address = static_cast<std::uint8_t>(reg);
        return (address <= 17 || address == 62 || address == 63) ? 2 : 0;
    }
    }
}
bool validAdc(const AdcConfiguration &adc) {
    return adc.mode <= 15 && adc.busTime <= 7 && adc.shuntTime <= 7 && adc.temperatureTime <= 7 &&
           adc.averaging <= 7;
}
std::uint16_t adcWord(const AdcConfiguration &adc) {
    return static_cast<std::uint16_t>((adc.mode << 12) | (adc.busTime << 9) | (adc.shuntTime << 6) |
                                      (adc.temperatureTime << 3) | adc.averaging);
}
std::int64_t signedValue(std::uint64_t value, unsigned bits) {
    const auto signedMagnitude = static_cast<std::int64_t>(value);
    return (value & (std::uint64_t(1) << (bits - 1))) ? signedMagnitude - (std::int64_t(1) << bits)
                                                      : signedMagnitude;
}
} // namespace
Status Device::readRegister(Register reg, std::uint64_t &value) {
    const auto bytes = width(reg);
    if (bytes == 0)
        return Status::INVALID_ARGUMENT;
    std::uint8_t tx[6] = {0}, rx[6] = {0};
    tx[0] = static_cast<std::uint8_t>((static_cast<std::uint8_t>(reg) << 2) | 1);
    if (!m_bus.transfer(tx, rx, bytes + 1))
        return Status::SPI_ERROR;
    std::uint64_t result = 0;
    for (std::size_t i = 1; i <= bytes; ++i)
        result = (result << 8) | rx[i];
    value = result;
    return Status::OK;
}
Status Device::write(Register reg, std::uint16_t value) {
    const std::uint8_t tx[3] = {static_cast<std::uint8_t>(static_cast<std::uint8_t>(reg) << 2),
                                static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)};
    std::uint8_t rx[3] = {0};
    return m_bus.transfer(tx, rx, 3) ? Status::OK : Status::SPI_ERROR;
}
Status Device::verifiedWrite(Register reg, std::uint16_t value, std::uint16_t mask) {
    auto status = write(reg, value);
    std::uint64_t actual = 0;
    if (status == Status::OK)
        status = readRegister(reg, actual);
    if (status == Status::OK && (actual & mask) != (value & mask))
        status = Status::VERIFY_ERROR;
    if (status != Status::OK)
        m_initialized = false;
    return status;
}
Status Device::initialize(const Configuration &config) {
    if (!std::isfinite(config.shuntOhms) || !std::isfinite(config.maxCurrent) || config.shuntOhms <= 0 ||
        config.maxCurrent <= 0 || !validAdc(config.adc) || config.shuntTempco > 0x3fff)
        return Status::INVALID_ARGUMENT;
    const double drop = config.shuntOhms * config.maxCurrent;
    if (drop > (config.narrowRange ? 0.04096 : 0.16384))
        return Status::INVALID_ARGUMENT;
    const double lsb = config.maxCurrent / 524288.0;
    const double calibration =
        std::round(13107.2e6 * lsb * config.shuntOhms * (config.narrowRange ? 4.0 : 1.0));
    if (!std::isfinite(calibration) || calibration < 1 || calibration > 32767)
        return Status::INVALID_ARGUMENT;
    m_initialized = false;
    std::uint64_t manufacturer = 0, identity = 0;
    auto status = readRegister(Register::MANUFACTURER_ID, manufacturer);
    if (status != Status::OK)
        return status;
    status = readRegister(Register::DEVICE_ID, identity);
    if (status != Status::OK)
        return status;
    if (manufacturer != 0x5449 || (identity >> 4) != 0x229)
        return Status::IDENTITY_MISMATCH;
    status = write(Register::CONFIG, 0x8000);
    if (status != Status::OK)
        return status;
    if (!m_delay.milliseconds(2))
        return Status::DELAY_ERROR;
    // The reset default is continuous conversion: stop before changing calibration.
    status = verifiedWrite(Register::ADC_CONFIG, 0);
    if (status != Status::OK)
        return status;
    const auto configWord = static_cast<std::uint16_t>((config.conversionDelaySteps << 6) |
                                                       (config.temperatureCompensation ? 0x20 : 0) |
                                                       (config.narrowRange ? 0x10 : 0));
    status = verifiedWrite(Register::CONFIG, configWord, 0x3ff0);
    if (status != Status::OK)
        return status;
    status = verifiedWrite(Register::SHUNT_CAL, static_cast<std::uint16_t>(calibration), 0x7fff);
    if (status != Status::OK)
        return status;
    status = verifiedWrite(Register::SHUNT_TEMPCO, config.shuntTempco, 0x3fff);
    if (status != Status::OK)
        return status;
    status = verifiedWrite(Register::ADC_CONFIG, adcWord(config.adc));
    if (status != Status::OK)
        return status;
    m_config = config;
    m_currentLsb = lsb;
    m_configWord = configWord;
    m_adcWord = adcWord(config.adc);
    m_initialized = true;
    return Status::OK;
}
Status Device::configureAdc(const AdcConfiguration &adc) {
    if (!validAdc(adc))
        return Status::INVALID_ARGUMENT;
    if (!m_initialized)
        return Status::INVALID_STATE;
    const auto word = adcWord(adc);
    const auto status = verifiedWrite(Register::ADC_CONFIG, word);
    if (status == Status::OK) {
        m_config.adc = adc;
        m_adcWord = word;
    }
    return status;
}
Status Device::stop() {
    auto adc = m_config.adc;
    adc.mode = 0;
    return configureAdc(adc);
}
Status Device::trigger() {
    if (!m_initialized || m_config.adc.mode == 0 || m_config.adc.mode >= 8)
        return Status::INVALID_STATE;
    return verifiedWrite(Register::ADC_CONFIG, m_adcWord);
}
Status Device::resetAccumulators() {
    if (!m_initialized)
        return Status::INVALID_STATE;
    // RSTACC initiates the reset; readback checks retained configuration, not elapsed energy.
    return verifiedWrite(Register::CONFIG, static_cast<std::uint16_t>(m_configWord | 0x4000), 0x3ff0);
}
Status Device::setAlertConfig(std::uint16_t bits) {
    if (bits & 0x0fff)
        return Status::INVALID_ARGUMENT;
    if (!m_initialized)
        return Status::INVALID_STATE;
    return verifiedWrite(Register::DIAG_ALRT, bits, 0xf000);
}
Status Device::setThreshold(Register reg, std::uint16_t raw) {
    const auto address = static_cast<std::uint8_t>(reg);
    if (address < 12 || address > 17 || ((reg == Register::BOVL || reg == Register::BUVL) && (raw & 0x8000)))
        return Status::INVALID_ARGUMENT;
    if (!m_initialized)
        return Status::INVALID_STATE;
    return verifiedWrite(reg, raw);
}
Status Device::read(Measurement &measurement) {
    const auto mode = m_config.adc.mode;
    if (!m_initialized || (mode & 7) == 0)
        return Status::INVALID_STATE;
    std::uint64_t diag = 0;
    auto status = readRegister(Register::DIAG_ALRT, diag);
    if (status != Status::OK)
        return status;
    if (!(diag & 1))
        return Status::DEVICE_ERROR;
    if (diag & 0x0e00)
        return Status::NUMERIC_OVERFLOW;
    if (!(diag & 2))
        return Status::NOT_READY;
    const Register registers[7] = {Register::VSHUNT, Register::VBUS,   Register::DIETEMP, Register::CURRENT,
                                   Register::POWER,  Register::ENERGY, Register::CHARGE};
    std::uint64_t raw[7] = {0};
    for (std::size_t i = 0; i < 7; ++i) {
        status = readRegister(registers[i], raw[i]);
        if (status != Status::OK)
            return status;
    }
    Measurement result;
    result.rawShunt = static_cast<std::int32_t>(signedValue(raw[0] >> 4, 20));
    result.rawBus = static_cast<std::uint32_t>(raw[1] >> 4);
    result.rawTemperature = static_cast<std::int16_t>(signedValue(raw[2], 16));
    result.rawCurrent = static_cast<std::int32_t>(signedValue(raw[3] >> 4, 20));
    result.rawPower = static_cast<std::uint32_t>(raw[4]);
    result.rawEnergy = raw[5];
    result.rawCharge = signedValue(raw[6], 40);
    result.busVolts = result.rawBus * 195.3125e-6;
    result.shuntVolts = result.rawShunt * (m_config.narrowRange ? 78.125e-9 : 312.5e-9);
    result.temperatureC = result.rawTemperature * 7.8125e-3;
    result.currentAmps = result.rawCurrent * m_currentLsb;
    result.powerWatts = result.rawPower * 3.2 * m_currentLsb;
    result.energyJoules = static_cast<double>(result.rawEnergy) * 51.2 * m_currentLsb;
    result.chargeCoulombs = static_cast<double>(result.rawCharge) * m_currentLsb;
    result.diagnostics = static_cast<std::uint16_t>(diag);
    result.validMask = static_cast<std::uint8_t>(mode & 7);
    if (mode & 2)
        result.validMask |= 8;
    if ((mode & 3) == 3)
        result.validMask |= 16;
    if ((mode & 8) && (mode & 3) == 3)
        result.validMask |= 32;
    if ((mode & 8) && (mode & 2))
        result.validMask |= 64;
    measurement = result;
    return Status::OK;
}
} // namespace Ina229
