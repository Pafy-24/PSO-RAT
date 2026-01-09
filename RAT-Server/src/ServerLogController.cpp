#include "ServerLogController.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>

namespace Server {

ServerLogController::ServerLogController(ServerManager *manager) 
    : IController(manager), manager(manager) {}

ServerLogController::~ServerLogController() { 
    stop(); 
}

void ServerLogController::start() {
    if (running) return;
    running = true;
}

void ServerLogController::stop() {
    running = false;
}

void ServerLogController::handle(const nlohmann::json &packet) {
    (void)packet;
}

void ServerLogController::pushLog(const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMtx);
    logs.push(msg);
    
    try {
        std::ofstream ofs(getLogPath(), std::ios::app);
        if (ofs) ofs << msg << std::endl;
    } catch (...) {}
}

bool ServerLogController::popLog(std::string &out) {
    std::lock_guard<std::mutex> lock(logMtx);
    if (logs.empty()) return false;
    out = std::move(logs.front());
    logs.pop();
    return true;
}

pid_t ServerLogController::spawnTerminal(const std::string &innerCmd) {
    auto which_ok = [](const char *name)->bool {
        std::string cmd = std::string("which ") + name + " >/dev/null 2>&1";
        return std::system(cmd.c_str()) == 0;
    };

    std::string prog;
    if (which_ok("qterminal")) prog = "qterminal";
    else if (which_ok("xfce4-terminal")) prog = "xfce4-terminal";
    else if (which_ok("konsole")) prog = "konsole";
    else if (which_ok("gnome-terminal")) prog = "gnome-terminal";
    else if (which_ok("xterm")) prog = "xterm";
    else if (which_ok("tmux")) prog = "tmux";
    else return -1;

    if (prog == "tmux") {
        std::string s = std::string("tmux new-session -d -s rat_server '") + innerCmd + "'";
        int rc = std::system(s.c_str());
        return rc == 0 ? 0 : -1;
    }

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        setsid();
        
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        
        if (prog == "qterminal") {
            execlp("qterminal", "qterminal", "-e", "bash", "-c", innerCmd.c_str(), (char*)NULL);
        } else if (prog == "xfce4-terminal") {
            execlp("xfce4-terminal", "xfce4-terminal", "-e", "bash", "-c", innerCmd.c_str(), (char*)NULL);
        } else if (prog == "konsole") {
            execlp("konsole", "konsole", "-e", "bash", "-c", innerCmd.c_str(), (char*)NULL);
        } else if (prog == "gnome-terminal") {
            execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c", innerCmd.c_str(), (char*)NULL);
        } else if (prog == "xterm") {
            execlp("xterm", "xterm", "-e", "bash", "-c", innerCmd.c_str(), (char*)NULL);
        }
        _exit(127);
    }

    return pid;
}

std::string ServerLogController::showLogs() {
    std::string cmd = "tail -F " + getLogPath() + "; exec bash";
    pid_t pid = spawnTerminal(cmd);
    
    if (pid == -1) {
        return "Error: Could not launch terminal emulator\n" +
               std::string("Log file: ") + getLogPath() + "\n" +
               "To view manually: tail -F " + getLogPath() + "\n";
    }
    
    if (pid == 0) {
        return "Launched log viewer in tmux session\n";
    }
    
    return "Launched log viewer in terminal (PID: " + std::to_string(pid) + ")\n";
}

}
