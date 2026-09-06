#pragma once
#include "Ina229/Device/Device.hpp"
namespace Ina229Bench {
// Host-only transport for the local Python SPI server; owns its connected socket.
class UnixSpi final : public Ina229::SpiBus {
  public:
    explicit UnixSpi(int connectedFd = -1, unsigned timeoutMs = 1000);
    ~UnixSpi();
    UnixSpi(const UnixSpi &) = delete;
    UnixSpi &operator=(const UnixSpi &) = delete;
    bool open(const char *path, unsigned timeoutMs = 1000);
    bool transfer(const std::uint8_t *, std::uint8_t *, std::size_t) override;

  private:
    void close();
    int m_fd;
    unsigned m_timeout;
};
} // namespace Ina229Bench
