#pragma once

#include <string>
#include <memory>
#include <thread>

#include <atomic>
#include <chrono>
#include "ServerController.hpp"

namespace Server {

class ServerManager;

class ServerLogController : public ServerController {
public:
    explicit ServerLogController(ServerManager *manager);
    ~ServerLogController();

    // start log thread
    void start() override;
    // stop log thread
    void stop() override;
    // spawn a separate program that runs the LogScript on the given log file
    // This launches a new process attached to a new pseudo-terminal and
    // returns the slave pty path (e.g. /dev/pts/N) on success or an empty
    // string on failure.
    std::string showLogs(const std::string &pathToLogFile);

    // Handle JSON commands directed to the log controller.
    // Recognized JSON: { "action": "show" , "path": "/path/to/log" }
    bool handleJson(const nlohmann::json &params) override;

private:
    ServerManager *manager_;
    bool running_ = false;
    std::unique_ptr<std::thread> thread_;
};

} // namespace Server
