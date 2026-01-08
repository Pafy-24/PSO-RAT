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

    
    ServerManager(const ServerManager &) = delete;
    ServerManager &operator=(const ServerManager &) = delete;

    bool start(unsigned short port);
    void stop();

    bool isRunning() const;

    
    
    int run(unsigned short port);

    
    
    bool addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket);
    
    bool removeClient(const std::string &deviceName);
    
    Utils::TCPSocket *getClient(const std::string &deviceName);
    
    IController *getController(const std::string &deviceName);

    ~ServerManager();

private:
    ServerManager();

    
    inline static std::shared_ptr<ServerManager> instance;
    
    
    inline static std::mutex initMutex;

    std::unique_ptr<Utils::TCPSocket> listener_;
    
    unsigned short port_ = 0;
    bool running_ = false;
    
    mutable std::mutex mtx_;
    
    std::map<std::string, std::unique_ptr<Utils::TCPSocket>> clients_;
    std::map<std::string, std::unique_ptr<IController>> controllers_;
    
    std::queue<std::string> logs_;
    mutable std::mutex logMtx_;
    
    std::string logPath_;

    

public:
    
    ServerController *getServerController(const std::string &deviceName);

public:
    
    void pushLog(const std::string &msg);
    bool popLog(std::string &out);

    
    std::vector<std::string> listClients();
    bool hasClient(const std::string &name);

    
    unsigned short port() const { return port_; }

    
    const std::string &logPath() const { return logPath_; }

    
    void spawnTerminalsForAdminAndLogs();
};

} 
