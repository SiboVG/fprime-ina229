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
    for (auto flags : {0U, 0x203U, 0x403U, 0x803U}) {
        b.registers[0xb] = flags;
        EXPECT_EQ(flags == 0 ? Status::DEVICE_ERROR : Status::NUMERIC_OVERFLOW, d.read(m));
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
    EXPECT_EQ(0xc000U, b.registers[0xb]);
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.setAlertConfig(1));
    EXPECT_EQ(Status::OK, d.setThreshold(Register::SOVL, 0xfffe));
    EXPECT_EQ(0xfffeU, b.registers[0xc]);
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.setThreshold(Register::VBUS, 1));
    EXPECT_EQ(Status::INVALID_ARGUMENT, d.setThreshold(Register::BOVL, 0x8000));
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
int main() {
    return Ina229Test::runAll();
}
