#pragma once

#include "Utils/TCPSocket.hpp"
#include <memory>
#include <mutex>

namespace Client {

class ClientManager {
public:
    static std::shared_ptr<ClientManager> getInstance();

    ClientManager(const ClientManager &) = delete;
    ClientManager &operator=(const ClientManager &) = delete;

    bool connectTo(const sf::IpAddress &address, unsigned short port);
    void disconnect();

    bool isConnected() const;

    ~ClientManager();

private:
    ClientManager();

    // inline static members to satisfy "static variables declared in header" requirement
    inline static std::shared_ptr<ClientManager> instance;
    // protects lazy initialization of the singleton instance in getInstance()
    // used for double-checked locking (check-then-lock-then-check)
    inline static std::mutex initMutex;

    std::unique_ptr<Utils::TCPSocket> socket_;
    // protects access to socket_ and other mutable internal state
    // ensures only one thread can modify connection state at a time
    mutable std::mutex mtx_;
};

} // namespace Client
