#pragma once
#include "Ina229/Device/Device.hpp"
#include <algorithm>
#include <array>
#include <vector>
namespace Ina229Test {
// Register emulator following datasheet SLYS023A section 7.6.1: reset values, writable masks,
// self-clearing RST/RSTACC, read-only result and ID registers, and the DIAG_ALRT read/clear rules.
// Tests may assign `registers` directly to inject device state.
class Bus final : public Ina229::SpiBus {
  public:
    std::array<std::uint64_t, 64> registers{};
    std::vector<std::vector<std::uint8_t>> transactions;
    int failAddress = -1;
    int failReadAddress = -1;
    int failWriteAddress = -1;
    int ignoreWriteAddress = -1;
    // Let this many writes to failWriteAddress succeed before failing.
    unsigned failWriteSkip = 0;
    bool stuckHigh = false;
    // Writes to read-only/unlisted registers or with a non-16-bit frame; the core must never do this.
    unsigned illegalWrites = 0;
    Bus() {
        defaults();
    }
    void defaults() {
        registers.fill(0);
        registers[0x01] = 0xfb68;
        registers[0x02] = 0x1000;
        registers[0x0b] = 0x0001;
        registers[0x0c] = 0x7fff;
        registers[0x0d] = 0x8000;
        registers[0x0e] = 0x7fff;
        registers[0x10] = 0x7fff;
        registers[0x11] = 0xffff;
        registers[0x3e] = 0x5449;
        registers[0x3f] = 0x2291;
    }
    static std::uint16_t writableMask(unsigned reg) {
        switch (reg) {
        case 0x00:
            return 0xfff0; // CONFIG: RST/RSTACC self-clear, bits 3..0 reserved (Table 7-5)
        case 0x01:
            return 0xffff; // ADC_CONFIG (Table 7-6)
        case 0x02:
            return 0x7fff; // SHUNT_CAL bit 15 reserved (Table 7-7)
        case 0x03:
            return 0x3fff; // SHUNT_TEMPCO bits 15..14 reserved (Table 7-8)
        case 0x0b:
            return 0xf0ff; // DIAG_ALRT: 11..9 read-only flags, 8 reserved (Table 7-16)
        case 0x0c:
        case 0x0d:
        case 0x10:
        case 0x11:
            return 0xffff; // SOVL, SUVL, TEMP_LIMIT, PWR_LIMIT (Tables 7-17, 7-18, 7-21, 7-22)
        case 0x0e:
        case 0x0f:
            return 0x7fff; // BOVL, BUVL bit 15 reserved (Tables 7-19, 7-20)
        default:
            return 0; // results, IDs and unlisted locations are read-only
        }
    }
    bool transfer(const std::uint8_t *tx, std::uint8_t *rx, std::size_t size) override {
        transactions.emplace_back(tx, tx + size);
        const auto reg = tx[0] >> 2;
        bool writeFault = !(tx[0] & 1) && reg == failWriteAddress;
        if (writeFault && failWriteSkip > 0) {
            --failWriteSkip;
            writeFault = false;
        }
        if (reg == failAddress || ((tx[0] & 1) ? reg == failReadAddress : writeFault))
            return false;
        std::fill(rx, rx + size, stuckHigh ? 0xff : 0);
        if (stuckHigh)
            return true;
        if (tx[0] & 1) {
            auto value = registers[reg];
            for (std::size_t i = size - 1; i > 0; --i) {
                rx[i] = value & 255;
                value >>= 8;
            }
            if (reg == 0xb)
                registers[reg] &= ~std::uint64_t(Ina229::Diag::CNVRF);
            if (reg == 9)
                registers[0xb] &= ~std::uint64_t(Ina229::Diag::ENERGYOF);
            if (reg == 10)
                registers[0xb] &= ~std::uint64_t(Ina229::Diag::CHARGEOF);
        } else if (reg != ignoreWriteAddress) {
            const auto mask = writableMask(reg);
            if (size != 3 || mask == 0) {
                ++illegalWrites;
                return true;
            }
            const std::uint64_t value = (std::uint64_t(tx[1]) << 8) | tx[2];
            if (reg == 0 && (value & 0x8000)) {
                defaults();
                return true;
            }
            if (reg == 0 && (value & 0x4000)) {
                registers[9] = 0;
                registers[10] = 0;
                registers[0xb] &= ~std::uint64_t(Ina229::Diag::MATHOF);
            }
            registers[reg] = (registers[reg] & ~std::uint64_t(mask)) | (value & mask);
            if (reg == 0)
                registers[0] &= ~std::uint64_t(0xc000);
            // Starting a conversion clears CNVRF and MATHOF (Table 7-16).
            if (reg == 1 && (value >> 12) != 0 && (value >> 12) != 8)
                registers[0xb] &= ~std::uint64_t(Ina229::Diag::CNVRF | Ina229::Diag::MATHOF);
        }
        return true;
    }
};
class Delay final : public Ina229::Delay {
  public:
    std::uint32_t elapsed = 0;
    bool succeed = true;
    bool milliseconds(std::uint32_t value) override {
        elapsed += value;
        return succeed;
    }
};
} // namespace Ina229Test
