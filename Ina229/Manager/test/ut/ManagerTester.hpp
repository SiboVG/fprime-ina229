#pragma once
#include "Ina229/Manager/Manager.hpp"
#include "Ina229/Manager/ManagerGTestBase.hpp"
#include "tests/TestSupport.hpp"
namespace Ina229 {
class ManagerTester final: public ManagerGTestBase {
 public:
    static const NATIVE_INT_TYPE TEST_INSTANCE_ID=0;
    ManagerTester();
    void nominal(); void failures(); void commands();
 private:
    void connectPorts(); void initComponents();
    TransferStatus from_spi_handler(NATIVE_INT_TYPE portNum,Fw::Buffer& tx,Fw::Buffer& rx) override;
    Ina229Test::Bus bus;
    bool failTransfer=false, shortTransfer=false;
    Manager component;
};
}
