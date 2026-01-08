#include "ServerLogController.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <queue>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
#include <pty.h>
#include <fcntl.h>

namespace Server {

ServerLogController::ServerLogController(ServerManager *manager) : IController(manager), manager_(manager) {}
ServerLogController::~ServerLogController() { stop(); }

void ServerLogController::handle(const nlohmann::json &packet) {
    
    (void)packet;
}

void ServerLogController::start() {
    if (running_) return;
    running_ = true;
    
}

void ServerLogController::stop() {
    running_ = false;
}

std::string ServerLogController::showLogs(const std::string &pathToLogFile) {
    
    int master = -1, slave = -1;
    char slaveName[128] = {0};
    if (openpty(&master, &slave, slaveName, NULL, NULL) < 0) {
        pushLog("ServerLogController::showLogs: openpty failed");
        return std::string();
    }

    pid_t pid = fork();
    if (pid < 0) {
        pushLog("ServerLogController::showLogs: fork failed");
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
    pushLog(std::string("ServerLogController::showLogs: spawned log_script pid=") + std::to_string(pid));
    return std::string(slaveName);
}

void ServerLogController::tailLogs(const std::string &pathToLogFile, int numLines) {
    std::string cmd = "tail -n " + std::to_string(numLines) + " " + pathToLogFile;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cout << "Failed to read log file: " << pathToLogFile << std::endl;
        return;
    }
    
    std::cout << "\n=== Last " << numLines << " lines from " << pathToLogFile << " ===\n" << std::endl;
    
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        std::cout << buf;
    }
    
    pclose(pipe);
    std::cout << "\n=== End of logs ===\n" << std::endl;
}

bool ServerLogController::handleJson(const nlohmann::json &params) {
    try {
        if (params.contains("action") && params["action"].is_string()) {
            std::string action = params["action"].get<std::string>();
            if (action == "show") {
                std::string path;
                if (params.contains("path") && params["path"].is_string()) path = params["path"].get<std::string>();
                if (path.empty()) path = logPath_.empty() ? std::string("/tmp/rat_server.log") : logPath_;
                std::string pty = showLogs(path);
                if (!pty.empty()) {
                    pushLog(std::string("LogScript spawned on ") + pty);
                    return true;
                }
                return false;
            }
        }
    } catch (...) {}
    return false;
}

void ServerLogController::pushLog(const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMtx_);
    logs_.push(msg);
    
    
    if (logPath_.empty()) logPath_ = "/tmp/rat_server.log";
    try {
        std::ofstream ofs(logPath_, std::ios::app);
        if (ofs) ofs << msg << std::endl;
    } catch (...) {}
}

bool ServerLogController::popLog(std::string &out) {
    std::lock_guard<std::mutex> lock(logMtx_);
    if (logs_.empty()) return false;
    out = std::move(logs_.front());
    logs_.pop();
    return true;
}

} 
