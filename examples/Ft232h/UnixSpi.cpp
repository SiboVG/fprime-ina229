// SPDX-License-Identifier: MIT
#include "UnixSpi.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
namespace Ina229Bench {
namespace {
using Clock = std::chrono::steady_clock;
bool ready(int fd, short events, Clock::time_point deadline) {
    while (Clock::now() < deadline) {
        const auto left =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        pollfd poller{fd, events, 0};
        const int result = ::poll(&poller, 1, static_cast<int>(left > 0 ? left : 1));
        if (result > 0)
            return (poller.revents & events) != 0;
        if (result == 0 || errno != EINTR)
            return false;
    }
    return false;
}
bool exchangeBytes(int fd, std::uint8_t *data, std::size_t size, bool send, Clock::time_point deadline) {
    std::size_t offset = 0;
    while (offset < size && ready(fd, send ? POLLOUT : POLLIN, deadline)) {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        const auto count = send ? ::send(fd, data + offset, size - offset, flags)
                                : ::recv(fd, data + offset, size - offset, 0);
        if (count > 0)
            offset += static_cast<std::size_t>(count);
        else if (count == 0 || (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK))
            return false;
    }
    return offset == size;
}
bool prepare(int fd) {
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return false;
#ifdef SO_NOSIGPIPE
    const int enabled = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) < 0)
        return false;
#endif
    return true;
}
} // namespace
UnixSpi::UnixSpi(int fd, unsigned timeout) : m_fd(fd), m_timeout(timeout) {
    if (m_fd >= 0 && (timeout == 0 || timeout > 60000 || !prepare(m_fd)))
        close();
}
UnixSpi::~UnixSpi() {
    close();
}
void UnixSpi::close() {
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
}
bool UnixSpi::open(const char *path, unsigned timeout) {
    if (m_fd >= 0 || !path || timeout == 0 || timeout > 60000)
        return false;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (std::strlen(path) == 0 || std::strlen(path) >= sizeof(address.sun_path))
        return false;
    std::memcpy(address.sun_path, path, std::strlen(path) + 1);
    m_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    m_timeout = timeout;
    if (m_fd < 0)
        return false;
    if (!prepare(m_fd)) {
        close();
        return false;
    }
    const int result = ::connect(m_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
    if (result < 0) {
        if (errno != EINPROGRESS ||
            !ready(m_fd, POLLOUT, Clock::now() + std::chrono::milliseconds(timeout))) {
            close();
            return false;
        }
        int error = 0;
        socklen_t size = sizeof(error);
        if (::getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error != 0) {
            close();
            return false;
        }
    }
    return true;
}
bool UnixSpi::transfer(const std::uint8_t *tx, std::uint8_t *rx, std::size_t size) {
    if (m_fd < 0 || !tx || !rx || size == 0 || size > 6)
        return false;
    std::uint8_t request[7] = {static_cast<std::uint8_t>(size)}, response[7] = {0};
    std::memcpy(request + 1, tx, size);
    const auto deadline = Clock::now() + std::chrono::milliseconds(m_timeout);
    if (!exchangeBytes(m_fd, request, size + 1, true, deadline) ||
        !exchangeBytes(m_fd, response, size + 1, false, deadline) || response[0] != 0) {
        // Drop ambiguous framing after any fault; never reuse a late response.
        close();
        return false;
    }
    std::memcpy(rx, response + 1, size);
    return true;
}
} // namespace Ina229Bench
