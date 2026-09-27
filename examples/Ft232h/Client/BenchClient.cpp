// SPDX-License-Identifier: MIT
#include "BenchClient.hpp"
namespace Ina229Bench {
Ina229::DriverStatus BenchClient::configureSensor(const Ina229::DeviceConfig &config) {
    return configure_out(0, config);
}
Ina229::DriverStatus BenchClient::readSensor(Ina229::Reading &reading) {
    return read_out(0, reading);
}
} // namespace Ina229Bench
