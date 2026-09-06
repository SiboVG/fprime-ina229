// SPDX-License-Identifier: MIT
#include "BridgeDriver.hpp"
namespace Ina229Bench {
Ina229::TransferStatus BridgeDriver::exchange_handler(FwIndexType, Fw::Buffer& tx, Fw::Buffer& rx) {
    if (tx.getSize() != rx.getSize()) return Ina229::TransferStatus::ERROR;
    return m_spi.transfer(tx.getData(), rx.getData(), tx.getSize()) ?
        Ina229::TransferStatus::OK : Ina229::TransferStatus::ERROR;
}
}
