#pragma once
#include "Client/BenchClientComponentAc.hpp"
namespace Ina229Bench {
class BenchClient final : public BenchClientComponentBase {
  public:
    explicit BenchClient(const char *name) : BenchClientComponentBase(name) {}
    Ina229::DriverStatus configureSensor(const Ina229::DeviceConfig &config);
    Ina229::DriverStatus readSensor(Ina229::Reading &reading);
};
} // namespace Ina229Bench
