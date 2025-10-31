#include "ServerManager.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <fstream>

namespace Server {


std::shared_ptr<ServerManager> ServerManager::getInstance() {
    if (!ServerManager::instance) {
        
        
        std::lock_guard<std::mutex> lock(ServerManager::initMutex);
        
        if (!ServerManager::instance) ServerManager::instance.reset(new ServerManager());
    }
    return ServerManager::instance;
}

ServerManager::ServerManager() = default;
ServerManager::~ServerManager() {
    std::lock_guard<std::mutex> lock(mtx_);
    stop();
    
        // stop controllers first to ensure their threads won't use sockets
        for (auto &p : controllers_) if (p.second) p.second->stop();
        controllers_.clear();

        // now close sockets (controllers no longer access them)
        for (auto &p : clients_) if (p.second) p.second->close();
        clients_.clear();
}

bool ServerManager::start(unsigned short port) {
    
    std::lock_guard<std::mutex> lock(mtx_);
    if (running_) return false;
    listener_ = std::make_unique<Utils::TCPSocket>();
    if (!listener_->bind(port)) {
        listener_.reset();
        return false;
    }
    port_ = port;
    running_ = true;
    
        controllers_.emplace("log", std::make_unique<ServerLogController>(this));
        controllers_.emplace("command", std::make_unique<ServerCommandController>(this));
        if (auto it = controllers_.find("log"); it != controllers_.end()) it->second->start();
        if (auto it = controllers_.find("command"); it != controllers_.end()) it->second->start();
    return true;
}


void ServerManager::stop() {
    
    std::lock_guard<std::mutex> lock(mtx_);
    if (!running_) return;
    if (listener_) listener_->close();
    listener_.reset();
    port_ = 0;
    running_ = false;
}

bool ServerManager::isRunning() const {
    
    std::lock_guard<std::mutex> lock(mtx_);
    return running_;
}

int ServerManager::run(unsigned short port) {
    if (!start(port)) return 1;
    
    int clientCount = 0;
    while (isRunning()) {
        auto client = listener_->accept();
        if (!client) {
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        const std::string device = "client" + std::to_string(++clientCount);
        Utils::TCPSocket *clientPtr = client.get();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            clients_[device] = std::move(client);
            
            controllers_[device] = std::make_unique<ServerController>(this, device, clientPtr);
            if (auto it = controllers_.find(device); it != controllers_.end()) it->second->start();
        }
    }
    
    return 0;
}

bool ServerManager::addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket) {
    
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket) return false;
    if (clients_.find(deviceName) != clients_.end()) return false; 
    Utils::TCPSocket *sockPtr = socket.get();
    clients_[deviceName] = std::move(socket);
    
    controllers_[deviceName] = std::make_unique<ServerController>(this, deviceName, sockPtr);
    if (auto it = controllers_.find(deviceName); it != controllers_.end()) it->second->start();
    return true;
}

bool ServerManager::removeClient(const std::string &deviceName) {
    
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) return false;
    
        // stop controller first to ensure background threads are joined
        if (auto cit = controllers_.find(deviceName); cit != controllers_.end()) {
            if (cit->second) cit->second->stop();
            controllers_.erase(cit);
        }

        if (it->second) it->second->close();
        clients_.erase(it);
        return true;
}

Utils::TCPSocket *ServerManager::getClient(const std::string &deviceName) {
    
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) return nullptr;
    return it->second.get(); 
}

IController *ServerManager::getController(const std::string &deviceName) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = controllers_.find(deviceName);
    if (it == controllers_.end()) return nullptr;
    return it->second.get();
}

ServerController *ServerManager::getServerController(const std::string &deviceName) {
    IController *ic = getController(deviceName);
    if (!ic) return nullptr;
    return dynamic_cast<ServerController *>(ic);
}

void ServerManager::pushLog(const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMtx_);
    logs_.push(msg);
    
    if (logPath_.empty()) logPath_ = "/tmp/rat_server.log";
    try {
        std::ofstream ofs(logPath_, std::ios::app);
        if (ofs) ofs << msg << std::endl;
    } catch (...) {}
}

bool ServerManager::popLog(std::string &out) {
    std::lock_guard<std::mutex> lock(logMtx_);
    if (logs_.empty()) return false;
    out = std::move(logs_.front());
    logs_.pop();
    return true;
}

std::vector<std::string> ServerManager::listClients() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> names;
    for (auto &p : clients_) names.push_back(p.first);
    return names;
}

bool ServerManager::hasClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx_);
    return clients_.find(name) != clients_.end();
}

} 
