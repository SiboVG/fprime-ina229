#pragma once
#include "Ina229/Manager/Manager.hpp"
#include "Ina229/Manager/ManagerGTestBase.hpp"
#include "tests/TestSupport.hpp"
namespace Ina229 {
class ManagerTester final : public ManagerGTestBase {
  public:
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;
    static const FwIndexType MAX_HISTORY_SIZE = 1000;
    ManagerTester();
    void nominal();
    void failures();
    void commands();
    void alertConfigurationKeepsReadsValid();
    void intermittentFaultsAreRateLimited();
    void validationErrorsAreNotDeviceFaults();
    void overflowDegradesAndRestores();

  private:
    void connectPorts();
    void initComponents();
    Drv::SpiStatus from_spi_handler(FwIndexType portNum, Fw::Buffer &tx, Fw::Buffer &rx) override;
    Ina229Test::Bus bus;
    bool failTransfer = false, shortTransfer = false;
    Manager component;
};
} // namespace Ina229
