// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace Ina229 {
enum class Status : std::uint8_t {
    OK,
    INVALID_ARGUMENT,
    INVALID_STATE,
    SPI_ERROR,
    IDENTITY_MISMATCH,
    VERIFY_ERROR,
    NOT_READY,
    DEVICE_ERROR,
    NUMERIC_OVERFLOW,
    DELAY_ERROR
};
enum class Register : std::uint8_t {
    CONFIG = 0,
    ADC_CONFIG = 1,
    SHUNT_CAL = 2,
    SHUNT_TEMPCO = 3,
    VSHUNT = 4,
    VBUS = 5,
    DIETEMP = 6,
    CURRENT = 7,
    POWER = 8,
    ENERGY = 9,
    CHARGE = 10,
    DIAG_ALRT = 11,
    SOVL = 12,
    SUVL = 13,
    BOVL = 14,
    BUVL = 15,
    TEMP_LIMIT = 16,
    PWR_LIMIT = 17,
    MANUFACTURER_ID = 62,
    DEVICE_ID = 63
};
// Field encodings are those of ADC_CONFIG; checked before any write.
struct AdcConfiguration {
    std::uint8_t mode = 15, busTime = 0, shuntTime = 0, temperatureTime = 0, averaging = 0;
};
struct Configuration {
    double shuntOhms = 0.01, maxCurrent = 8.0;
    bool narrowRange = false, temperatureCompensation = false;
    std::uint8_t conversionDelaySteps = 0;
    std::uint16_t shuntTempco = 0;
    AdcConfiguration adc;
};
struct Measurement {
    std::int32_t rawShunt = 0, rawCurrent = 0;
    std::uint32_t rawBus = 0, rawPower = 0;
    std::int16_t rawTemperature = 0;
    std::uint64_t rawEnergy = 0;
    std::int64_t rawCharge = 0;
    double busVolts = 0, shuntVolts = 0, temperatureC = 0, currentAmps = 0;
    double powerWatts = 0, energyJoules = 0, chargeCoulombs = 0;
    std::uint16_t diagnostics = 0;
    // bits: bus, shunt, temperature, current, power, energy, charge.
    std::uint8_t validMask = 0;
};
class SpiBus {
  public:
    virtual ~SpiBus() = default;
    // One synchronous, full-duplex, MSB-first transaction under one CS assertion.
    // Return true only after exactly size bytes complete. No pointers are retained.
    virtual bool transfer(const std::uint8_t *tx, std::uint8_t *rx, std::size_t size) = 0;
};
class Delay {
  public:
    virtual ~Delay() = default;
    virtual bool milliseconds(std::uint32_t duration) = 0;
};
class Device final {
  public:
    Device(SpiBus &bus, Delay &delay) : m_bus(bus), m_delay(delay) {}
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Status initialize(const Configuration &config);
    Status configureAdc(const AdcConfiguration &adc);
    Status stop();
    Status trigger();
    Status resetAccumulators();
    Status setAlertConfig(std::uint16_t bits);
    Status setThreshold(Register reg, std::uint16_t raw);
    Status read(Measurement &measurement);
    // Diagnostic access; DIAG_ALRT, ENERGY and CHARGE have read side effects.
    Status readRegister(Register reg, std::uint64_t &value);
    bool isInitialized() const {
        return m_initialized;
    }
    double currentLsb() const {
        return m_currentLsb;
    }

  private:
    Status write(Register reg, std::uint16_t value);
    Status verifiedWrite(Register reg, std::uint16_t value, std::uint16_t mask = 0xffff);
    SpiBus &m_bus;
    Delay &m_delay;
    Configuration m_config;
    bool m_initialized = false;
    double m_currentLsb = 0;
    std::uint16_t m_configWord = 0, m_adcWord = 0;
};
} // namespace Ina229
