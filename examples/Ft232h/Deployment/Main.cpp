// SPDX-License-Identifier: MIT
#include "Deployment/Top/BenchTopologyAc.hpp"
#include "Fw/Time/TimeInterval.hpp"
#include "Os/Task.hpp"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
namespace {
bool number(const char *text, double &value) {
    char *end = nullptr;
    errno = 0;
    value = std::strtod(text, &end);
    return end != text && *end == '\0' && errno == 0 && std::isfinite(value) && value > 0;
}
bool waitMs(unsigned ms) {
    return Os::Task::delay(Fw::TimeInterval(ms / 1000, (ms % 1000) * 1000)) == Os::Task::OP_OK;
}
} // namespace
int main(int argc, char **argv) {
    double shunt = 0, current = 0, requested = 10;
    if (argc < 4 || argc > 5 || !number(argv[2], shunt) || !number(argv[3], current) ||
        (argc == 5 && !number(argv[4], requested)) || requested > 100000 ||
        std::floor(requested) != requested) {
        std::cerr << "Usage: " << argv[0] << " SOCKET SHUNT_OHMS MAX_CURRENT_A [COUNT=10]\n";
        return 2;
    }
    Ina229Bench::TopologyState state;
    Ina229Bench::initComponents(state);
    Ina229Bench::setBaseIds();
    Ina229Bench::connectComponents();
    if (!Ina229Bench::spi.open(argv[1])) {
        std::cerr << "Cannot connect to SPI bridge\n";
        return 1;
    }
    Ina229::DeviceConfig config(shunt, current, false, false, 0, 0, 15, 0, 0, 0, 0);
    const auto configured = Ina229Bench::sensor.get_configure_InputPort(0)->invoke(config);
    if (configured != Ina229::DriverStatus::OK) {
        std::cerr << "INA229 initialization failed, status=" << configured.e << '\n';
        return 1;
    }
    std::cout << "sample,validMask,bus_V,shunt_V,temperature_C,current_A,power_W,energy_J,charge_C\n";
    std::cout << std::setprecision(12);
    for (unsigned i = 0; i < static_cast<unsigned>(requested); ++i) {
        Ina229::Reading reading;
        Ina229::DriverStatus status = Ina229::DriverStatus::NOT_READY;
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            status = Ina229Bench::sensor.get_read_InputPort(0)->invoke(reading);
            if (status != Ina229::DriverStatus::NOT_READY)
                break;
            if (!waitMs(10)) {
                std::cerr << "Delay failed\n";
                return 1;
            }
        }
        if (status != Ina229::DriverStatus::OK) {
            std::cerr << "INA229 acquisition failed, status=" << status.e << '\n';
            return 1;
        }
        std::cout << i << ',' << static_cast<unsigned>(reading.getvalidMask()) << ',' << reading.getbusVolts()
                  << ',' << reading.getshuntVolts() << ',' << reading.gettemperatureC() << ','
                  << reading.getcurrentAmps() << ',' << reading.getpowerWatts() << ','
                  << reading.getenergyJoules() << ',' << reading.getchargeCoulombs() << std::endl;
        if (i + 1 < static_cast<unsigned>(requested) && !waitMs(100))
            return 1;
    }
    return 0;
}
