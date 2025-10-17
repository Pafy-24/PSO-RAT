#pragma once

namespace Utils {

class Socket {
public:
    virtual ~Socket() = default;

    // Bind the socket to a local port (for servers/listeners).
    virtual bool bind(unsigned short port) = 0;

    // Close or shutdown the socket.
    virtual void close() = 0;
};

} // namespace Utils
