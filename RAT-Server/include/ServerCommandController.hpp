#pragma once

#include "ServerController.hpp"
#include <string>
#include <memory>
#include <thread>
#include <sys/types.h>

namespace Server {

class ServerManager;

class ServerCommandController : public ServerController {
public:
    explicit ServerCommandController(ServerManager *manager);
    ~ServerCommandController();

    
    void start() override;
    void stop() override;
    
    std::string getHandle() const override { return "command"; }

private:
    ServerManager *manager_;
    bool running_ = false;
    std::unique_ptr<std::thread> stdinThread_;
    std::string selectedClient_;

    pid_t spawnTerminal(const std::string &innerCmd);
    std::string processLine(const std::string &line);
    void stdinLoop();

    std::string handleHelp();
    std::string handleList();
    std::string handleChoose(const std::string &name);
    std::string handleKill(const std::string &name);
    std::string handleBash(const std::string &name, const std::string &cmd);
    std::string handleQuit();
    std::string handleShowLogs();
};

} 
