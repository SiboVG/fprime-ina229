#include "ManagerTester.hpp"
namespace Ina229 {
namespace {
DeviceConfig config() {
    return DeviceConfig(0.01, 8.0, false, false, 0, 0, 15, 0, 0, 0, 0);
}
} // namespace
ManagerTester::ManagerTester() : ManagerGTestBase("Tester", MAX_HISTORY_SIZE), component("Manager") {
    initComponents();
    connectPorts();
}
Drv::SpiStatus ManagerTester::from_spi_handler(FwIndexType, Fw::Buffer &tx, Fw::Buffer &rx) {
    if (failTransfer)
        return Drv::SpiStatus::SPI_OTHER_ERR;
    if (shortTransfer) {
        rx.setSize(0);
        return Drv::SpiStatus::SPI_OK;
    }
    return bus.transfer(tx.getData(), rx.getData(), tx.getSize()) ? Drv::SpiStatus::SPI_OK
                                                                  : Drv::SpiStatus::SPI_OTHER_ERR;
}
void ManagerTester::nominal() {
    EXPECT_EQ(DriverStatus::OK, invoke_to_configure(0, config()));
    bus.registers[0xb] = 3;
    bus.registers[5] = 5120ULL << 4;
    Reading r;
    EXPECT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    EXPECT_DOUBLE_EQ(1, r.get_busVolts());
    EXPECT_EQ(127, r.get_validMask());
    EXPECT_EQ(DriverStatus::NOT_READY, invoke_to_read(0, r));
    EXPECT_DOUBLE_EQ(1, r.get_busVolts());
    ASSERT_EVENTS_SIZE(0);
    EXPECT_EQ(0U, bus.illegalWrites);
}
void ManagerTester::failures() {
    Reading r;
    r.set_busVolts(42);
    EXPECT_EQ(DriverStatus::INVALID_STATE, invoke_to_read(0, r));
    failTransfer = true;
    EXPECT_EQ(DriverStatus::SPI_ERROR, invoke_to_configure(0, config()));
    failTransfer = false;
    EXPECT_EQ(DriverStatus::OK, invoke_to_configure(0, config()));
    clearHistory();
    failTransfer = true;
    EXPECT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    EXPECT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    EXPECT_DOUBLE_EQ(42, r.get_busVolts());
    ASSERT_EVENTS_OperationFailed_SIZE(1);
    ASSERT_EVENTS_OperationFailed(0, Operation::READ, DriverStatus::SPI_ERROR);
    failTransfer = false;
    shortTransfer = true;
    EXPECT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    shortTransfer = false;
    bus.registers[0xb] = 3;
    EXPECT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    ASSERT_EVENTS_Recovered_SIZE(1);
    ASSERT_EVENTS_Recovered(0, Operation::READ);
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
    ASSERT_CMD_RESPONSE_SIZE(10);
    ASSERT_CMD_RESPONSE(4, Manager::OPCODE_READ_SNAPSHOT, 5, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(7, Manager::OPCODE_RESET_ACCUMULATORS, 8, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(8, Manager::OPCODE_STOP, 9, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(9, Manager::OPCODE_REPORT, 10, Fw::CmdResponse::OK);
    bus.ignoreWriteAddress = 2;
    c.set_maxCurrent(4);
    sendCmd_INITIALIZE(0, 11, c);
    ASSERT_CMD_RESPONSE(10, Manager::OPCODE_INITIALIZE, 11, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_OperationFailed_SIZE(2);
    ASSERT_EVENTS_OperationFailed(0, Operation::READ, DriverStatus::INVALID_STATE);
    ASSERT_EVENTS_OperationFailed(1, Operation::INITIALIZE, DriverStatus::VERIFY_ERROR);
    sendCmd_REPORT(0, 12);
    ASSERT_TLM_Initialized(1, 0);
    clearHistory();
    sendCmd_READ_SNAPSHOT(0, 13);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Manager::OPCODE_READ_SNAPSHOT, 13, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_TLM_Measurement_SIZE(0);
    EXPECT_EQ(0U, bus.illegalWrites);
}
void ManagerTester::alertConfigurationKeepsReadsValid() {
    sendCmd_INITIALIZE(0, 1, config());
    sendCmd_SET_ALERT_CONFIG(0, 2, 0x8000);
    ASSERT_CMD_RESPONSE(1, Manager::OPCODE_SET_ALERT_CONFIG, 2, Fw::CmdResponse::OK);
    bus.registers[0xb] |= 2;
    sendCmd_READ_SNAPSHOT(0, 3);
    ASSERT_CMD_RESPONSE(2, Manager::OPCODE_READ_SNAPSHOT, 3, Fw::CmdResponse::OK);
    ASSERT_TLM_Measurement_SIZE(1);
    ASSERT_EVENTS_OperationFailed_SIZE(0);
}
void ManagerTester::intermittentFaultsAreRateLimited() {
    ASSERT_EQ(DriverStatus::OK, invoke_to_configure(0, config()));
    clearHistory();
    Reading r;
    const U32 cycles = 100;
    for (U32 i = 0; i < cycles; ++i) {
        failTransfer = true;
        ASSERT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
        failTransfer = false;
        bus.registers[0xb] = 3;
        ASSERT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    }
    // Every edge is counted, but at most the FPP throttle (10) of each event is emitted.
    ASSERT_EVENTS_OperationFailed_SIZE(10);
    ASSERT_EVENTS_Recovered_SIZE(10);
    sendCmd_REPORT(0, 1);
    ASSERT_TLM_Errors(0, static_cast<U64>(cycles));
    sendCmd_CLEAR_EVENT_THROTTLE(0, 2);
    ASSERT_CMD_RESPONSE(1, Manager::OPCODE_CLEAR_EVENT_THROTTLE, 2, Fw::CmdResponse::OK);
    failTransfer = true;
    ASSERT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    ASSERT_EVENTS_OperationFailed_SIZE(11);
    // A successful initialization also re-enables throttled events.
    for (U32 i = 0; i < 20; ++i) {
        failTransfer = false;
        bus.registers[0xb] = 3;
        ASSERT_EQ(DriverStatus::OK, invoke_to_read(0, r));
        failTransfer = true;
        ASSERT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    }
    ASSERT_EVENTS_OperationFailed_SIZE(20);
    failTransfer = false;
    ASSERT_EQ(DriverStatus::OK, invoke_to_configure(0, config()));
    failTransfer = true;
    ASSERT_EQ(DriverStatus::SPI_ERROR, invoke_to_read(0, r));
    ASSERT_EVENTS_OperationFailed_SIZE(21);
}
void ManagerTester::validationErrorsAreNotDeviceFaults() {
    auto invalid = config();
    invalid.set_shuntOhms(0);
    sendCmd_INITIALIZE(0, 1, invalid);
    ASSERT_CMD_RESPONSE(0, Manager::OPCODE_INITIALIZE, 1, Fw::CmdResponse::VALIDATION_ERROR);
    EXPECT_TRUE(bus.transactions.empty());
    sendCmd_INITIALIZE(0, 2, config());
    sendCmd_SET_THRESHOLD(0, 3, 4, 0);
    ASSERT_CMD_RESPONSE(2, Manager::OPCODE_SET_THRESHOLD, 3, Fw::CmdResponse::VALIDATION_ERROR);
    sendCmd_SET_ALERT_CONFIG(0, 4, 0x0001);
    ASSERT_CMD_RESPONSE(3, Manager::OPCODE_SET_ALERT_CONFIG, 4, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_OperationFailed_SIZE(0);
    sendCmd_REPORT(0, 5);
    ASSERT_TLM_Errors(0, 0U);
    ASSERT_TLM_Initialized(0, 1);
    // TRIGGER in a continuous mode is out of sequence, not a device fault.
    sendCmd_TRIGGER(0, 6);
    ASSERT_CMD_RESPONSE(5, Manager::OPCODE_TRIGGER, 6, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_OperationFailed_SIZE(0);
    sendCmd_REPORT(0, 7);
    ASSERT_TLM_Errors(1, 0U);
}
void ManagerTester::overflowDegradesAndRestores() {
    ASSERT_EQ(DriverStatus::OK, invoke_to_configure(0, config()));
    Reading r;
    bus.registers[0xb] = Ina229::Diag::MATHOF | 3;
    ASSERT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    EXPECT_EQ(7, r.get_validMask());
    ASSERT_EVENTS_MeasurementDegraded_SIZE(1);
    ASSERT_EVENTS_MeasurementDegraded(0, 7, 127);
    bus.registers[0xb] = 3;
    ASSERT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    EXPECT_EQ(31, r.get_validMask());
    ASSERT_EVENTS_MeasurementDegraded_SIZE(2);
    bus.registers[0xb] = 3;
    ASSERT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    ASSERT_EVENTS_MeasurementDegraded_SIZE(2);
    sendCmd_RESET_ACCUMULATORS(0, 1);
    ASSERT_CMD_RESPONSE(0, Manager::OPCODE_RESET_ACCUMULATORS, 1, Fw::CmdResponse::OK);
    bus.registers[0xb] = 3;
    ASSERT_EQ(DriverStatus::OK, invoke_to_read(0, r));
    EXPECT_EQ(127, r.get_validMask());
    ASSERT_EVENTS_MeasurementRestored_SIZE(1);
    ASSERT_EVENTS_MeasurementRestored(0, 127);
    ASSERT_EVENTS_OperationFailed_SIZE(0);
}
} // namespace Ina229
