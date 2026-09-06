#include "ManagerTester.hpp"
namespace Ina229 {
namespace {
DeviceConfig config() {
    return DeviceConfig(0.01, 8.0, false, false, 0, 0, 15, 0, 0, 0, 0);
}
} // namespace
ManagerTester::ManagerTester() : ManagerGTestBase("Tester", 100), component("Manager") {
    initComponents();
    connectPorts();
}
TransferStatus ManagerTester::from_spi_handler(NATIVE_INT_TYPE, Fw::Buffer &tx, Fw::Buffer &rx) {
    if (failTransfer)
        return TransferStatus::ERROR;
    if (shortTransfer) {
        rx.setSize(0);
        return TransferStatus::OK;
    }
    return bus.transfer(tx.getData(), rx.getData(), tx.getSize()) ? TransferStatus::OK
                                                                  : TransferStatus::ERROR;
}
void ManagerTester::nominal() {
    auto c = config();
    EXPECT_EQ(DriverStatus::OK, invoke_to_configure(0, c));
    bus.registers[0xb] = 3;
    bus.registers[5] = 5120ULL << 4;
    Reading r;
    EXPECT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    EXPECT_DOUBLE_EQ(1, r.getbusVolts());
    EXPECT_EQ(127, r.getvalidMask());
    EXPECT_EQ(DriverStatus::NOT_READY, invoke_to_read(0, r));
    EXPECT_DOUBLE_EQ(1, r.getbusVolts());
}
void ManagerTester::failures() {
    Reading r;
    r.setbusVolts(42);
    EXPECT_EQ(DriverStatus::INVALID_STATE, invoke_to_read(0, r));
    auto c = config();
    failTransfer = true;
    EXPECT_EQ(DriverStatus::SPI_ERROR, invoke_to_configure(0, c));
    failTransfer = false;
    EXPECT_EQ(DriverStatus::OK, invoke_to_configure(0, c));
    clearHistory();
    failTransfer = true;
    EXPECT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    EXPECT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    EXPECT_DOUBLE_EQ(42, r.getbusVolts());
    ASSERT_EVENTS_OperationFailed_SIZE(1);
    failTransfer = false;
    shortTransfer = true;
    EXPECT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    shortTransfer = false;
    bus.registers[0xb] = 3;
    EXPECT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    ASSERT_EVENTS_Recovered_SIZE(1);
}
void ManagerTester::commands() {
    auto c = config();
    sendCmd_INITIALIZE(0, 1, c);
    ASSERT_CMD_RESPONSE(0, Manager::OPCODE_INITIALIZE, 1, Fw::CmdResponse::OK);
    sendCmd_CONFIGURE_ADC(0, 2, 16, 0, 0, 0, 0);
    ASSERT_CMD_RESPONSE(1, Manager::OPCODE_CONFIGURE_ADC, 2, Fw::CmdResponse::VALIDATION_ERROR);
    sendCmd_CONFIGURE_ADC(0, 3, 7, 0, 0, 0, 0);
    ASSERT_CMD_RESPONSE(2, Manager::OPCODE_CONFIGURE_ADC, 3, Fw::CmdResponse::OK);
    sendCmd_TRIGGER(0, 4);
    ASSERT_CMD_RESPONSE(3, Manager::OPCODE_TRIGGER, 4, Fw::CmdResponse::OK);
    bus.registers[0xb] = 3;
    sendCmd_READ_SNAPSHOT(0, 5);
    ASSERT_TLM_Measurement_SIZE(1);
    sendCmd_SET_ALERT_CONFIG(0, 6, 0xc000);
    ASSERT_CMD_RESPONSE(5, Manager::OPCODE_SET_ALERT_CONFIG, 6, Fw::CmdResponse::OK);
    sendCmd_SET_THRESHOLD(0, 7, 12, 10);
    ASSERT_CMD_RESPONSE(6, Manager::OPCODE_SET_THRESHOLD, 7, Fw::CmdResponse::OK);
    bus.registers[9] = 99;
    sendCmd_RESET_ACCUMULATORS(0, 8);
    EXPECT_EQ(0U, bus.registers[9]);
    sendCmd_STOP(0, 9);
    Reading r;
    EXPECT_EQ(DriverStatus::INVALID_STATE, invoke_to_read(0, r));
    sendCmd_REPORT(0, 10);
    ASSERT_TLM_Initialized_SIZE(1);
    ASSERT_TLM_Samples(0, 1U);
}
} // namespace Ina229
