#pragma once

#include "Utils/TCPSocket.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <string>
namespace Server {

class ServerManager {
public:
    static std::shared_ptr<ServerManager> getInstance();

    // non-copyable
    ServerManager(const ServerManager &) = delete;
    ServerManager &operator=(const ServerManager &) = delete;

    bool start(unsigned short port);
    void stop();

    bool isRunning() const;

    // Client management
    // register a new client socket under a device name (takes ownership)
    bool addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket);
    // remove and close a client socket by device name
    bool removeClient(const std::string &deviceName);
    // get a raw pointer to a client socket (non-owning). Returns nullptr if not found.
    Utils::TCPSocket *getClient(const std::string &deviceName);

    ~ServerManager();

private:
    ServerManager();

    // inline static members to satisfy "static variables declared in header" requirement
    inline static std::shared_ptr<ServerManager> instance;
    // protects lazy initialization of the singleton instance in getInstance()
    // used for double-checked locking (check-then-lock-then-check)
    inline static std::mutex initMutex;

    std::unique_ptr<Utils::TCPSocket> listener_;
    bool running_ = false;
    // protects listener_, running_ and other mutable state inside ServerManager
    mutable std::mutex mtx_;
    // map of connected devices: device name -> socket
    // Access to this map must be protected by mtx_ (or a dedicated mutex if you prefer)
    std::map<std::string, std::unique_ptr<Utils::TCPSocket>> clients_;
};

} // namespace Server
