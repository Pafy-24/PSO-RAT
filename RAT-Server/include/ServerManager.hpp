#pragma once

#include "Utils/TCPSocket.hpp"
#include "ServerController.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <string>
#include <queue>
#include <vector>

#include "ServerCommandController.hpp"
#include "ServerLogController.hpp"
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

    // Run the server: start listening and accept a single connection then echo a JSON message.
    // Returns 0 on success, non-zero on failure.
    int run(unsigned short port);

    // Client management
    // register a new client socket under a device name (takes ownership)
    bool addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket);
    // remove and close a client socket by device name
    bool removeClient(const std::string &deviceName);
    // get a raw pointer to a client socket (non-owning). Returns nullptr if not found.
    Utils::TCPSocket *getClient(const std::string &deviceName);
    // access controller for a connected client
    ServerController *getController(const std::string &deviceName);

    ~ServerManager();

private:
    ServerManager();

    // inline static members to satisfy "static variables declared in header" requirement
    inline static std::shared_ptr<ServerManager> instance;
    // protects lazy initialization of the singleton instance in getInstance()
    // used for double-checked locking (check-then-lock-then-check)
    inline static std::mutex initMutex;

    std::unique_ptr<Utils::TCPSocket> listener_;
    // port that server is listening on (set in start())
    unsigned short port_ = 0;
    bool running_ = false;
    // protects listener_, running_ and other mutable state inside ServerManager
    mutable std::mutex mtx_;
    // map of connected devices: device name -> socket
    // Access to this map must be protected by mtx_ (or a dedicated mutex if you prefer)
    std::map<std::string, std::unique_ptr<Utils::TCPSocket>> clients_;
    // controllers backed by the client sockets (stored as polymorphic controllers)
    std::map<std::string, std::unique_ptr<IController>> controllers_;

    // log queue
    std::queue<std::string> logs_;
    mutable std::mutex logMtx_;
    // path to server log file (used when opening a logs terminal)
    std::string logPath_;

    // (command/log controllers will be stored in the polymorphic controllers_ map)

public:
    // Returns the ServerController for a client if present, otherwise nullptr
    ServerController *getServerController(const std::string &deviceName);

public:
    // logging API used by other classes
    void pushLog(const std::string &msg);
    bool popLog(std::string &out);

    // client helpers exposed to controllers
    std::vector<std::string> listClients();
    bool hasClient(const std::string &name);

    // access configured port
    unsigned short port() const { return port_; }

    // access configured log path
    const std::string &logPath() const { return logPath_; }

    // attempt to open two terminals: one tails logs, the other connects to admin port
    void spawnTerminalsForAdminAndLogs();
};

} // namespace Server
