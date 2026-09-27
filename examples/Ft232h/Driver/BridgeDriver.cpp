// SPDX-License-Identifier: MIT
#include "BridgeDriver.hpp"
namespace Ina229Bench {
Drv::SpiStatus BridgeDriver::exchange_handler(FwIndexType, Fw::Buffer &tx, Fw::Buffer &rx) {
    if (tx.getSize() != rx.getSize())
        return Drv::SpiStatus::SPI_MISMATCH_ERR;
    return m_spi.transfer(tx.getData(), rx.getData(), tx.getSize()) ? Drv::SpiStatus::SPI_OK
                                                                    : Drv::SpiStatus::SPI_OTHER_ERR;
}
} // namespace Ina229Bench
