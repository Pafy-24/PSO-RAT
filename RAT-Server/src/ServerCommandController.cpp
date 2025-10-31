#include "ServerCommandController.hpp"
#include "ServerManager.hpp"
#include "ServerLogController.hpp"
#include <iostream>
#include <sstream>
#include "Utils/TCPSocket.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>
#include <nlohmann/json.hpp>

namespace Server {

ServerCommandController::ServerCommandController(ServerManager *manager) : ServerController(nullptr), manager_(manager) {}
ServerCommandController::~ServerCommandController() { stop(); }

void ServerCommandController::start() {
    if (running_) return;
    running_ = true;

    // Only use existing client connections managed by ServerManager; rely on stdin for admin input
    if (isatty(STDIN_FILENO)) {
        stdinThread_ = std::make_unique<std::thread>(&ServerCommandController::stdinLoop, this);
    }
}

// spawn a terminal emulator (or tmux) to run innerCmd; returns pid (>0) for forked child,
// 0 when launched in tmux, -1 on failure
pid_t ServerCommandController::spawnTerminal(const std::string &innerCmd) {
    auto which_ok = [](const char *name)->bool {
        std::string cmd = std::string("which ") + name + " >/dev/null 2>&1";
        return std::system(cmd.c_str()) == 0;
    };

    std::string prog;
    if (which_ok("gnome-terminal")) prog = "gnome-terminal";
    else if (which_ok("konsole")) prog = "konsole";
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
        if (prog == "gnome-terminal") {
            execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        } else if (prog == "konsole") {
            execlp("konsole", "konsole", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        } else if (prog == "xterm") {
            execlp("xterm", "xterm", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        }
        _exit(127);
    }

    return pid;
}

std::string ServerCommandController::processLine(const std::string &line) {
    std::istringstream iss(line);
    std::string cmd; iss >> cmd;
    std::string reply;
    if (cmd == "list") {
        return handleList();
    } else if (cmd == "choose") {
        std::string name; iss >> name;
        return handleChoose(name);
    } else if (cmd == "kill") {
        std::string name; iss >> name;
        return handleKill(name);
    } else if (cmd == "bash") {
        std::string name; iss >> name;
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty() && rest.front() == ' ') rest.erase(0,1);
        return handleBash(name, rest);
    } else if (cmd == "quit") {
        return handleQuit();
    } else if (cmd == "showlogs") {
        return handleShowLogs();
    }
    return std::string("Unknown command\n");
}

std::string ServerCommandController::handleList() {
    std::string reply;
    auto clients = manager_->listClients();
    for (auto &c : clients) reply += c + "\n";
    return reply;
}

std::string ServerCommandController::handleChoose(const std::string &name) {
    return manager_->hasClient(name) ? std::string("Chosen ") + name + "\n" : std::string("No such client\n");
}

std::string ServerCommandController::handleKill(const std::string &name) {
    if (manager_->removeClient(name)) return std::string("Killed ") + name + "\n";
    return std::string("No such client\n");
}

std::string ServerCommandController::handleBash(const std::string &name, const std::string &cmd) {
    IController *ctrl = nullptr;
    if (manager_) ctrl = manager_->getController(name);
    if (!ctrl) return std::string("No such client\n");
    std::string reply = "Sending bash command...\n";
    nlohmann::json out{ {"cmd", cmd} };
    ctrl->sendJson(out);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string lg;
    while (manager_->popLog(lg)) reply += lg + "\n";
    return reply;
}

std::string ServerCommandController::handleQuit() {
    running_ = false;
    if (manager_) manager_->stop();
    return std::string("Quitting\n");
}

std::string ServerCommandController::handleShowLogs() {
    ServerController *lc = nullptr;
    if (manager_) lc = manager_->getServerController("log");
    if (!lc) return std::string("No log controller\n");
    auto *slc = dynamic_cast<ServerLogController *>(lc);
    if (!slc) return std::string("Invalid log controller\n");
    std::string path = manager_->logPath();
    if (path.empty()) path = "/tmp/rat_server.log";
    std::string inner = std::string("../build_make/log_script ") + path + std::string("; exec bash");
    pid_t p = spawnTerminal(inner);
    if (p == -1) return std::string("No terminal emulator available\n");
    if (p == 0) return std::string("Launched in tmux session\n");
    return std::string("Launched terminal pid=") + std::to_string(p) + "\n";
}

// adminLoop removed: we use existing client connections via ServerManager and stdin for commands

void ServerCommandController::stdinLoop() {
    std::string line;
    while (running_ && std::getline(std::cin, line)) {
        std::string reply = processLine(line);
        if (!reply.empty()) std::cout << reply << std::flush;
    }
}

void ServerCommandController::stop() {
    if (!running_) return;
    running_ = false;
    if (stdinThread_ && stdinThread_->joinable()) stdinThread_->join();
}

} 

