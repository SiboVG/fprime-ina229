#pragma once
#include "Ina229/Device/Device.hpp"
#include <array>
#include <vector>
#include <algorithm>
namespace Ina229Test {
class Bus final : public Ina229::SpiBus {
 public:
    std::array<std::uint64_t, 64> registers{};
    std::vector<std::vector<std::uint8_t>> transactions;
    int failAddress = -1;
    int ignoreWriteAddress = -1;
    Bus() { defaults(); }
    void defaults() {
        registers.fill(0); registers[0x3e]=0x5449; registers[0x3f]=0x2291;
        registers[1]=0xfb68; registers[0xb]=1;
    }
    bool transfer(const std::uint8_t* tx, std::uint8_t* rx, std::size_t size) override {
        transactions.emplace_back(tx,tx+size);
        const auto reg=tx[0]>>2;
        if (reg==failAddress) return false;
        std::fill(rx,rx+size,0);
        if (tx[0]&1) {
            auto value=registers[reg];
            for (std::size_t i=size-1;i>0;--i) {rx[i]=value&255;value>>=8;}
            if(reg==0xb) registers[reg]&=~std::uint64_t(2);
            if(reg==9) registers[0xb]&=~std::uint64_t(0x800);
            if(reg==10) registers[0xb]&=~std::uint64_t(0x400);
        } else if (reg!=ignoreWriteAddress) {
            std::uint64_t value=0;
            for(std::size_t i=1;i<size;++i) value=(value<<8)|tx[i];
            if(reg==0 && (value&0x8000)) { defaults(); return true; }
            if(reg==0 && (value&0x4000)) {registers[9]=0;registers[10]=0;value&=~0x4000ULL;}
            registers[reg]=value;
            if(reg==1 && (value>>12)!=0 && (value>>12)!=8) registers[0xb]&=~std::uint64_t(2);
        }
        return true;
    }
};
class Delay final : public Ina229::Delay {
 public: std::uint32_t elapsed=0;
    bool milliseconds(std::uint32_t value) override {elapsed+=value;return true;}
};
}
