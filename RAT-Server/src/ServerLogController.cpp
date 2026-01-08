#include "ServerLogController.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <queue>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
#include <pty.h>
#include <fcntl.h>

namespace Server {

ServerLogController::ServerLogController(ServerManager *manager) : ServerController(nullptr), manager_(manager) {}
ServerLogController::~ServerLogController() { stop(); }

void ServerLogController::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
}

void ServerLogController::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_ && thread_->joinable()) thread_->join();
}

std::string ServerLogController::showLogs(const std::string &pathToLogFile) {
    
    int master = -1, slave = -1;
    char slaveName[128] = {0};
    if (openpty(&master, &slave, slaveName, NULL, NULL) < 0) {
        if (manager_) manager_->pushLog("ServerLogController::showLogs: openpty failed");
        return std::string();
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (manager_) manager_->pushLog("ServerLogController::showLogs: fork failed");
        close(master); 
        close(slave);
        return std::string();
    }
    if (pid == 0) {
        
        
        setsid();
        
        close(master);
        if (dup2(slave, STDIN_FILENO) < 0) _exit(127);
        if (dup2(slave, STDOUT_FILENO) < 0) _exit(127);
        if (dup2(slave, STDERR_FILENO) < 0) _exit(127);
        if (slave > STDERR_FILENO) close(slave);

        
        const char *prog = "../build_make/log_script";
        char *const argv[] = { (char*)prog, (char*)pathToLogFile.c_str(), NULL };
        execv(prog, argv);
        
        std::cerr << "Failed to exec " << prog << ": " << std::strerror(errno) << std::endl;
        _exit(127);
    }

    
    close(master);
    
    
    close(slave);
    if (manager_) manager_->pushLog(std::string("ServerLogController::showLogs: spawned log_script pid=") + std::to_string(pid));
    return std::string(slaveName);
}

bool ServerLogController::handleJson(const nlohmann::json &params) {
    try {
        if (params.contains("action") && params["action"].is_string()) {
            std::string action = params["action"].get<std::string>();
            if (action == "show") {
                std::string path;
                if (params.contains("path") && params["path"].is_string()) path = params["path"].get<std::string>();
                if (path.empty() && manager_) path = manager_->logPath().empty() ? std::string("/tmp/rat_server.log") : manager_->logPath();
                std::string pty = showLogs(path);
                if (!pty.empty()) {
                    if (manager_) manager_->pushLog(std::string("LogScript spawned on ") + pty);
                    return true;
                }
                return false;
            }
        }
    } catch (...) {}
    return false;
}

} 
