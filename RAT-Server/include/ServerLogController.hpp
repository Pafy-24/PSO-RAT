#pragma once

#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include "IController.hpp"

namespace Server {

class ServerManager;

class ServerLogController : public IController {
public:
    explicit ServerLogController(ServerManager *manager);
    ~ServerLogController();

    void start() override;
    void stop() override;
    void handle(const nlohmann::json &packet) override;
    std::string getHandle() const override { return "log"; }
    
    void pushLog(const std::string &msg);
    bool popLog(std::string &out);
    const std::string &getLogPath() const { return logPath; }
    
    std::string showLogs();

private:
    pid_t spawnTerminal(const std::string &cmd);

    ServerManager *manager;
    bool running = false;
    
    std::queue<std::string> logs;
    mutable std::mutex logMtx;
    std::string logPath = "/tmp/rat_server.log";
};

}
