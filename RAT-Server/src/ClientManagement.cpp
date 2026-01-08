#include "ClientManagement.hpp"
#include "ServerManager.hpp"
#include "ServerLogController.hpp"
#include <iostream>

namespace Server {

ClientManagement::ClientManagement(ServerManager* manager) : manager_(manager) {}

ClientManagement::~ClientManagement() {
    cleanup();
}





bool ClientManagement::addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket) return false;
    
    std::string clientIP = socket->getRemoteAddress();
    clients_[deviceName] = std::move(socket);
    clientIPs_[deviceName] = clientIP;
    logMessage("Client connected: " + deviceName + " (" + clientIP + ")");
    return true;
}

bool ClientManagement::removeClient(const std::string &deviceName) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) return false;
    
    if (it->second) it->second->close();
    clients_.erase(it);
    clientIPs_.erase(deviceName);
    return true;
}

bool ClientManagement::hasClient(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    return clients_.find(name) != clients_.end();
}





Utils::TCPSocket* ClientManagement::getClient(const std::string &deviceName) const {
    std::lock_guard<std::mutex> lock(mtx_);
    
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

std::vector<std::string> ClientManagement::listClients() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> names;
    for (auto &p : clients_) {
        names.push_back(p.first);
    }
    return names;
}

std::string ClientManagement::getClientIP(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clientIPs_.find(name);
    return (it != clientIPs_.end()) ? it->second : "unknown";
}





void ClientManagement::cleanup() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    
    for (auto &p : clients_) {
        if (p.second) {
            p.second->close();
        }
    }
    clients_.clear();
    clientIPs_.clear();
}





std::string ClientManagement::generateUniqueDeviceName(const std::string& base, int& counter) const {
    
    
    std::string name = base;
    while (clients_.find(name) != clients_.end()) {
        name = base + "_" + std::to_string(++counter);
    }
    return name;
}

void ClientManagement::logMessage(const std::string &msg) {
    if (!manager_) return;
    if (auto log = manager_->getSystemController("log")) {
        static_cast<ServerLogController*>(log)->pushLog(msg);
    }
}

} 
