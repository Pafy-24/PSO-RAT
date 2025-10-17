#pragma once

#include "Utils/TCPSocket.hpp"
#include "ClientController.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <string>

namespace Client {

class ClientManager {
public:
    static std::shared_ptr<ClientManager> getInstance();

    ClientManager(const ClientManager &) = delete;
    ClientManager &operator=(const ClientManager &) = delete;

    // Connect to the given address and create a controller with the given name.
    bool connectTo(const std::string &name, const sf::IpAddress &address, unsigned short port);
    // Disconnect and remove controller by name.
    void disconnect(const std::string &name);

    bool isConnected(const std::string &name) const;

    // controller access
    ClientController *getController(const std::string &name);

    // Run the client: connect to server and perform a simple handshake (blocking).
    // Returns 0 on success, non-zero on failure.
    int run(const sf::IpAddress &address, unsigned short port);

    ~ClientManager();

private:
    ClientManager();

    // inline static members to satisfy "static variables declared in header" requirement
    inline static std::shared_ptr<ClientManager> instance;
    // protects lazy initialization of the singleton instance in getInstance()
    // used for double-checked locking (check-then-lock-then-check)
    inline static std::mutex initMutex;

    // map of controller name -> controller (controllers reference the single socket_)
    std::map<std::string, ClientController> controllers_;
    // single shared socket to the server (shared with controllers)
    std::shared_ptr<Utils::TCPSocket> socket_;
    // protects access to maps and internal state
    mutable std::mutex mtx_;
};

} // namespace Client
