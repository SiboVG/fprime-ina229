#pragma once
#include "Driver/BridgeDriverComponentAc.hpp"
#include "examples/Ft232h/UnixSpi.hpp"
namespace Ina229Bench {
class BridgeDriver final: public BridgeDriverComponentBase {
 public:
    explicit BridgeDriver(const char* name):BridgeDriverComponentBase(name) {}
    bool open(const char* path){return m_spi.open(path,1000);}
 private:
    Ina229::TransferStatus exchange_handler(FwIndexType,Fw::Buffer&,Fw::Buffer&) override;
    UnixSpi m_spi;
};
}
