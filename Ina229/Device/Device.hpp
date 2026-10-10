// SPDX-License-Identifier: MIT
#ifndef INA229_DEVICE_DEVICE_HPP
#define INA229_DEVICE_DEVICE_HPP
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
// DIAG_ALRT fields, datasheet SLYS023A Table 7-16.
namespace Diag {
constexpr std::uint16_t MEMSTAT = 0x0001, CNVRF = 0x0002, MATHOF = 0x0200, CHARGEOF = 0x0400,
                        ENERGYOF = 0x0800, CONFIG_BITS = 0xf000;
}
// Measurement::validMask bits.
namespace Valid {
constexpr std::uint8_t BUS = 1, SHUNT = 2, TEMPERATURE = 4, CURRENT = 8, POWER = 16, ENERGY = 32, CHARGE = 64;
}
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
    // Valid:: bits. A clear bit means the value is stale or untrustworthy.
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
    // Channels the configured ADC mode produces; read() reports a subset when degraded.
    std::uint8_t expectedValidMask() const;
    // ENERGY/CHARGE bits latched invalid until resetAccumulators() or initialize().
    std::uint8_t invalidAccumulators() const {
        return m_invalidAccumulators;
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
    std::uint8_t m_invalidAccumulators = 0;
};
} // namespace Ina229
#endif // INA229_DEVICE_DEVICE_HPP
