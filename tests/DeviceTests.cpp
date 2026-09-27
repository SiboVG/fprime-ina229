#include "TestHarness.hpp"
#include "TestSupport.hpp"
#include <limits>
using namespace Ina229;
using Ina229Test::Bus;
using TestDelay = Ina229Test::Delay;
INA229_TEST(ProtocolWidthsAndByteOrder) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    std::uint64_t value = 0;
    for (auto reg : {Register::MANUFACTURER_ID, Register::VBUS, Register::ENERGY}) {
        b.registers[static_cast<unsigned>(reg)] = 0x123456789aULL;
        EXPECT_EQ(Status::OK, d.readRegister(reg, value));
        const auto &tx = b.transactions.back();
        EXPECT_EQ((static_cast<unsigned>(reg) << 2) | 1, tx[0]);
        EXPECT_EQ(reg == Register::ENERGY ? 6U : reg == Register::VBUS ? 4U : 3U, tx.size());
        EXPECT_EQ(reg == Register::ENERGY ? 0x123456789aULL
                  : reg == Register::VBUS ? 0x56789aULL
                                          : 0x789aULL,
                  value);
    }
    const auto before = b.transactions.size();
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.readRegister(static_cast<Register>(0x20), value));
    EXPECT_EQ(before, b.transactions.size());
}
INA229_TEST(InitializeValidatesIdentityAndCalibrationBeforeMeasurement) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    Configuration c;
    EXPECT_EQ(Status::OK, d.initialize(c));
    EXPECT_TRUE(d.isInitialized());
    EXPECT_EQ(2U, delay.elapsed);
    EXPECT_EQ(2000U, b.registers[2]);
    EXPECT_EQ(0xf000U, b.registers[1]);
    EXPECT_NEAR(8.0 / 524288, d.currentLsb(), 1e-15);
    bool stopped = false;
    for (const auto &t : b.transactions) {
        if (t[0] == 4 && t[1] == 0)
            stopped = true;
        if (t[0] == 8)
            EXPECT_TRUE(stopped);
    }
    c.narrowRange = true;
    c.maxCurrent = 2;
    EXPECT_EQ(Status::OK, d.initialize(c));
    EXPECT_EQ(2000U, b.registers[2]);
    EXPECT_EQ(0x10U, b.registers[0]);
    EXPECT_EQ(0U, b.illegalWrites);
    b.registers[0x3f] = 0x2280;
    EXPECT_EQ(Status::IDENTITY_MISMATCH, d.initialize(c));
    EXPECT_FALSE(d.isInitialized());
}
INA229_TEST(InvalidConfigurationDoesNoIo) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    for (double invalid :
         {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        Configuration c;
        c.shuntOhms = invalid;
        EXPECT_EQ(Status::INVALID_ARGUMENT, d.initialize(c));
    }
    Configuration c;
    c.maxCurrent = 20;
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.initialize(c));
    c = Configuration();
    c.adc.averaging = 8;
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.initialize(c));
    c = Configuration();
    c.shuntTempco = 0x4000;
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.initialize(c));
    EXPECT_TRUE(b.transactions.empty());
}
INA229_TEST(SignedMeasurementsAndScaling) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    b.registers[0xb] = 3;
    b.registers[4] = 0xfffff0;
    b.registers[5] = 5120ULL << 4;
    b.registers[6] = 0xff80;
    b.registers[7] = 0x800000;
    b.registers[8] = 100;
    b.registers[9] = 0xffffffffffULL;
    b.registers[10] = 0x8000000000ULL;
    Measurement m;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(-1, m.rawShunt);
    EXPECT_EQ(-524288, m.rawCurrent);
    EXPECT_EQ(-549755813888LL, m.rawCharge);
    EXPECT_NEAR(1.0, m.busVolts, 1e-12);
    EXPECT_NEAR(-312.5e-9, m.shuntVolts, 1e-15);
    EXPECT_NEAR(-8.0, m.currentAmps, 1e-12);
    EXPECT_NEAR(-1.0, m.temperatureC, 1e-12);
    EXPECT_NEAR(100 * 3.2 * d.currentLsb(), m.powerWatts, 1e-12);
    EXPECT_NEAR(1099511627775.0 * 51.2 * d.currentLsb(), m.energyJoules, 1e-6);
    EXPECT_NEAR(-549755813888.0 * d.currentLsb(), m.chargeCoulombs, 1e-6);
    EXPECT_EQ(127U, m.validMask);
}
INA229_TEST(ReadFailuresPreserveOutputAndDoNotClaimFreshness) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    Measurement m;
    m.busVolts = 42;
    EXPECT_EQ(Status::INVALID_STATE, d.read(m));
    EXPECT_TRUE(b.transactions.empty());
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    EXPECT_EQ(Status::NOT_READY, d.read(m));
    EXPECT_EQ(42, m.busVolts);
    // MEMSTAT = 0 (trim checksum error) and the always-zero DIAG_ALRT bit 8 reject the whole read.
    for (auto flags : {0x0U, 0x2U, 0x103U}) {
        b.registers[0xb] = flags;
        EXPECT_EQ(Status::DEVICE_ERROR, d.read(m));
        EXPECT_EQ(42, m.busVolts);
    }
    b.registers[0xb] = 3;
    b.failAddress = 7;
    EXPECT_EQ(Status::SPI_ERROR, d.read(m));
    EXPECT_EQ(42, m.busVolts);
}
INA229_TEST(ConfigurationFaultsInvalidateState) {
    for (int reg : {0, 1, 2, 3}) {
        Bus b;
        TestDelay delay;
        Device d(b, delay);
        Configuration c;
        c.shuntTempco = 10;
        EXPECT_EQ(Status::OK, d.initialize(c));
        b.ignoreWriteAddress = reg;
        c.narrowRange = true;
        c.maxCurrent = 1;
        c.shuntTempco = 20;
        EXPECT_EQ(Status::VERIFY_ERROR, d.initialize(c));
        EXPECT_FALSE(d.isInitialized());
    }
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    b.failAddress = 2;
    EXPECT_EQ(Status::SPI_ERROR, d.initialize(Configuration()));
    EXPECT_FALSE(d.isInitialized());
}
INA229_TEST(AdcModesValidityAndTriggering) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    Configuration c;
    EXPECT_EQ(Status::OK, d.initialize(c));
    for (unsigned mode = 0; mode < 16; ++mode) {
        AdcConfiguration adc;
        adc.mode = mode;
        EXPECT_EQ(Status::OK, d.configureAdc(adc));
        b.registers[0xb] = 3;
        Measurement m;
        EXPECT_EQ((mode & 7) == 0 ? Status::INVALID_STATE : Status::OK, d.read(m));
        if ((mode & 7) != 0) {
            EXPECT_EQ(d.expectedValidMask(), m.validMask);
            EXPECT_EQ((mode & 1) != 0, (m.validMask & 1) != 0);
            EXPECT_EQ((mode & 2) != 0, (m.validMask & 2) != 0);
            EXPECT_EQ((mode & 4) != 0, (m.validMask & 4) != 0);
            EXPECT_EQ((mode & 8) != 0 && (mode & 3) == 3, (m.validMask & 32) != 0);
        }
        EXPECT_EQ(mode > 0 && mode < 8 ? Status::OK : Status::INVALID_STATE, d.trigger());
    }
    EXPECT_EQ(Status::OK, d.stop());
    b.registers[0xb] = 3;
    Measurement m;
    EXPECT_EQ(Status::INVALID_STATE, d.read(m));
}
INA229_TEST(AccumulatorResetAndAlertControls) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    Configuration c;
    c.conversionDelaySteps = 3;
    EXPECT_EQ(Status::OK, d.initialize(c));
    b.registers[9] = 99;
    b.registers[10] = 88;
    EXPECT_EQ(Status::OK, d.resetAccumulators());
    EXPECT_EQ(0U, b.registers[9]);
    EXPECT_EQ(0xc0U, b.registers[0]);
    EXPECT_EQ(Status::OK, d.setAlertConfig(0xc000));
    EXPECT_EQ(0xc001U, b.registers[0xb]);
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.setAlertConfig(1));
    EXPECT_EQ(Status::OK, d.setThreshold(Register::SOVL, 0xfffe));
    EXPECT_EQ(0xfffeU, b.registers[0xc]);
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.setThreshold(Register::VBUS, 1));
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.setThreshold(Register::BOVL, 0x8000));
    EXPECT_EQ(Status::OK, d.setThreshold(Register::BOVL, 0x7fff));
    // TEMP_LIMIT is a full 16-bit two's-complement field (datasheet 7.6.1.17).
    EXPECT_EQ(Status::OK, d.setThreshold(Register::TEMP_LIMIT, 0xfff5));
    EXPECT_EQ(0xfff5U, b.registers[0x10]);
    EXPECT_EQ(0U, b.illegalWrites);
}
INA229_TEST(InitializationFaultMatrixAndInvalidLifecycle) {
    for (int reg : {0, 1, 2, 3, 62, 63}) {
        Bus b;
        TestDelay delay;
        Device d(b, delay);
        b.failReadAddress = reg;
        EXPECT_EQ(Status::SPI_ERROR, d.initialize(Configuration()));
        EXPECT_FALSE(d.isInitialized());
    }
    for (int reg : {0, 1, 2, 3}) {
        Bus b;
        TestDelay delay;
        Device d(b, delay);
        b.failWriteAddress = reg;
        EXPECT_EQ(Status::SPI_ERROR, d.initialize(Configuration()));
        EXPECT_FALSE(d.isInitialized());
    }
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    delay.succeed = false;
    EXPECT_EQ(Status::DELAY_ERROR, d.initialize(Configuration()));
    EXPECT_FALSE(d.isInitialized());
    EXPECT_EQ(Status::INVALID_STATE, d.configureAdc(AdcConfiguration()));
    EXPECT_EQ(Status::INVALID_STATE, d.resetAccumulators());
    EXPECT_EQ(Status::INVALID_STATE, d.setAlertConfig(0));
    EXPECT_EQ(Status::INVALID_STATE, d.setThreshold(Register::SOVL, 0));
    delay.succeed = true;
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    b.ignoreWriteAddress = 1;
    EXPECT_EQ(Status::VERIFY_ERROR, d.stop());
    EXPECT_FALSE(d.isInitialized());
    Measurement m;
    EXPECT_EQ(Status::INVALID_STATE, d.read(m));
}
INA229_TEST(ConfigurationBoundariesAndPositiveNarrowScaling) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    for (double value :
         {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        Configuration c;
        c.maxCurrent = value;
        EXPECT_EQ(Status::INVALID_ARGUMENT, d.initialize(c));
    }
    Configuration c;
    c.shuntOhms = 1e-12;
    c.maxCurrent = 1;
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.initialize(c));
    AdcConfiguration adc;
    adc.busTime = 8;
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.configureAdc(adc));
    EXPECT_TRUE(b.transactions.empty());
    c = Configuration();
    c.narrowRange = true;
    c.maxCurrent = 2;
    c.conversionDelaySteps = 255;
    c.shuntTempco = 0x3fff;
    c.temperatureCompensation = true;
    EXPECT_EQ(Status::OK, d.initialize(c));
    EXPECT_EQ(0x3ff0U, b.registers[0]);
    b.registers[11] = 3;
    b.registers[4] = 0x7ffff0;
    b.registers[7] = 0x7ffff0;
    b.registers[6] = 0x7fff;
    b.registers[10] = 0x7fffffffffULL;
    Measurement m;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_NEAR(524287 * 78.125e-9, m.shuntVolts, 1e-12);
    EXPECT_NEAR(524287 * d.currentLsb(), m.currentAmps, 1e-12);
    EXPECT_EQ(549755813887LL, m.rawCharge);
    EXPECT_NEAR(32767 * 7.8125e-3, m.temperatureC, 1e-12);
    EXPECT_EQ(Status::NOT_READY, d.read(m));
}
INA229_TEST(EveryAcquisitionTransferFailurePreservesOutput) {
    for (int reg : {4, 5, 6, 7, 8, 9, 10, 11}) {
        Bus b;
        TestDelay delay;
        Device d(b, delay);
        EXPECT_EQ(Status::OK, d.initialize(Configuration()));
        b.registers[11] = 3;
        b.failReadAddress = reg;
        Measurement m;
        m.busVolts = 42;
        m.validMask = 0x55;
        EXPECT_EQ(Status::SPI_ERROR, d.read(m));
        EXPECT_EQ(42, m.busVolts);
        EXPECT_EQ(0x55U, m.validMask);
    }
}
INA229_TEST(OverflowInvalidatesOnlyAffectedChannelsAndLatches) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    const std::uint8_t all = 127;
    Measurement m;
    // ENERGYOF: energy latched invalid; the read of ENERGY clears the device flag.
    b.registers[0xb] = Diag::ENERGYOF | Diag::CNVRF | Diag::MEMSTAT;
    b.registers[5] = 5120ULL << 4;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(all & ~Valid::ENERGY, m.validMask);
    EXPECT_NEAR(1.0, m.busVolts, 1e-12);
    EXPECT_EQ(0U, b.registers[0xb] & Diag::ENERGYOF);
    b.registers[0xb] = Diag::CNVRF | Diag::MEMSTAT;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(all & ~Valid::ENERGY, m.validMask);
    // CHARGEOF adds charge to the latch.
    b.registers[0xb] = Diag::CHARGEOF | Diag::CNVRF | Diag::MEMSTAT;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(all & ~(Valid::ENERGY | Valid::CHARGE), m.validMask);
    EXPECT_EQ(Valid::ENERGY | Valid::CHARGE, d.invalidAccumulators());
    EXPECT_EQ(Status::OK, d.resetAccumulators());
    EXPECT_EQ(0U, d.invalidAccumulators());
    b.registers[0xb] = Diag::CNVRF | Diag::MEMSTAT;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(all, m.validMask);
    // MATHOF: current and power invalid for this conversion; accumulators latched invalid.
    b.registers[0xb] = Diag::MATHOF | Diag::CNVRF | Diag::MEMSTAT;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(Valid::BUS | Valid::SHUNT | Valid::TEMPERATURE, m.validMask);
    b.registers[0xb] = Diag::CNVRF | Diag::MEMSTAT;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(all & ~(Valid::ENERGY | Valid::CHARGE), m.validMask);
    // Reinitialization resets the device and the latch.
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    b.registers[0xb] = Diag::CNVRF | Diag::MEMSTAT;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(all, m.validMask);
    EXPECT_EQ(0U, b.illegalWrites);
}
INA229_TEST(OverflowSeenBeforeTransferFailureStaysLatched) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    b.registers[0xb] = Diag::ENERGYOF | Diag::CNVRF | Diag::MEMSTAT;
    b.failReadAddress = 10; // ENERGY is read (clearing ENERGYOF), then CHARGE fails.
    Measurement m;
    m.validMask = 0x55;
    EXPECT_EQ(Status::SPI_ERROR, d.read(m));
    EXPECT_EQ(0x55U, m.validMask);
    EXPECT_EQ(0U, b.registers[0xb] & Diag::ENERGYOF);
    b.failReadAddress = -1;
    b.registers[0xb] = Diag::CNVRF | Diag::MEMSTAT;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(127 & ~Valid::ENERGY, m.validMask);
}
INA229_TEST(ResetAccumulatorsDrainsStaleOverflowFlags) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    // Overflow occurred but was never observed by read(); reset must not leave it to re-latch.
    b.registers[0xb] |= Diag::ENERGYOF | Diag::CHARGEOF | Diag::MATHOF;
    EXPECT_EQ(Status::OK, d.resetAccumulators());
    EXPECT_EQ(0U, b.registers[0xb] & (Diag::ENERGYOF | Diag::CHARGEOF | Diag::MATHOF));
    b.registers[0xb] |= Diag::CNVRF;
    Measurement m;
    EXPECT_EQ(Status::OK, d.read(m));
    EXPECT_EQ(127U, m.validMask);
    // A failed drain keeps the latch and reports the transfer failure.
    b.registers[0xb] = Diag::ENERGYOF | Diag::CNVRF | Diag::MEMSTAT;
    EXPECT_EQ(Status::OK, d.read(m));
    b.failReadAddress = 10;
    EXPECT_EQ(Status::SPI_ERROR, d.resetAccumulators());
    EXPECT_EQ(Valid::ENERGY, d.invalidAccumulators());
    EXPECT_TRUE(d.isInitialized());
}
INA229_TEST(AlertConfigurationPreservesTrimStatusAndReadsSucceed) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    EXPECT_EQ(Status::OK, d.setAlertConfig(0x8000));
    EXPECT_EQ(Diag::MEMSTAT, b.registers[0xb] & Diag::MEMSTAT);
    b.registers[0xb] |= Diag::CNVRF;
    Measurement m;
    EXPECT_EQ(Status::OK, d.read(m));
    // A real checksum error stays visible after a configuration write.
    b.registers[0xb] &= ~std::uint64_t(Diag::MEMSTAT);
    EXPECT_EQ(Status::OK, d.setAlertConfig(0x4000));
    b.registers[0xb] |= Diag::CNVRF;
    EXPECT_EQ(Status::DEVICE_ERROR, d.read(m));
    b.failReadAddress = 11;
    EXPECT_EQ(Status::SPI_ERROR, d.setAlertConfig(0));
    EXPECT_TRUE(d.isInitialized());
    EXPECT_EQ(0U, b.illegalWrites);
}
INA229_TEST(ImplausibleBusDataIsRejected) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    Measurement m;
    m.busVolts = 42;
    b.stuckHigh = true;
    EXPECT_EQ(Status::DEVICE_ERROR, d.read(m));
    EXPECT_EQ(42, m.busVolts);
    EXPECT_EQ(0U, d.invalidAccumulators());
    b.stuckHigh = false;
    for (int reg : {4, 5, 7}) {
        b.registers[0xb] = Diag::CNVRF | Diag::MEMSTAT;
        b.registers[reg] = 0x000011;
        EXPECT_EQ(Status::DEVICE_ERROR, d.read(m));
        EXPECT_EQ(42, m.busVolts);
        b.registers[reg] = 0;
    }
}
INA229_TEST(RemainingValidationAndFaultBranches) {
    Bus b;
    TestDelay delay;
    Device d(b, delay);
    EXPECT_EQ(0U, d.expectedValidMask());
    EXPECT_EQ(Status::INVALID_STATE, d.trigger());
    AdcConfiguration adc;
    for (int field = 0; field < 5; ++field) {
        adc = AdcConfiguration();
        if (field == 0)
            adc.mode = 16;
        if (field == 1)
            adc.busTime = 8;
        if (field == 2)
            adc.shuntTime = 8;
        if (field == 3)
            adc.temperatureTime = 8;
        if (field == 4)
            adc.averaging = 8;
        EXPECT_EQ(Status::INVALID_ARGUMENT, d.configureAdc(adc));
    }
    EXPECT_TRUE(b.transactions.empty());
    b.registers[0x3e] = 0;
    EXPECT_EQ(Status::IDENTITY_MISMATCH, d.initialize(Configuration()));
    b.registers[0x3e] = 0x5449;
    // The stop write succeeds; the final ADC_CONFIG write fails.
    b.failWriteAddress = 1;
    b.failWriteSkip = 1;
    EXPECT_EQ(Status::SPI_ERROR, d.initialize(Configuration()));
    EXPECT_FALSE(d.isInitialized());
    b.failWriteAddress = -1;
    EXPECT_EQ(Status::OK, d.initialize(Configuration()));
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.setThreshold(static_cast<Register>(18), 0));
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.setThreshold(Register::BUVL, 0x8000));
    b.failReadAddress = 9;
    EXPECT_EQ(Status::SPI_ERROR, d.resetAccumulators());
    EXPECT_TRUE(d.isInitialized());
    b.failReadAddress = -1;
    b.ignoreWriteAddress = 0;
    b.registers[0] = 0x1230;
    EXPECT_EQ(Status::VERIFY_ERROR, d.resetAccumulators());
    EXPECT_FALSE(d.isInitialized());
    EXPECT_EQ(0U, b.illegalWrites);
}
int main() {
    return Ina229Test::runAll();
}
