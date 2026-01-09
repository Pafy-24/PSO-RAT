#include "ClientManagement.hpp"
#include "ServerManager.hpp"
#include <iostream>

namespace Server {

ClientManagement::ClientManagement(ServerManager* manager) : manager(manager) {}

ClientManagement::~ClientManagement() {
    cleanup();
}





bool ClientManagement::addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!socket) return false;
    
    std::string clientIP = socket->getRemoteAddress();
    clients[deviceName] = std::move(socket);
    clientIPs[deviceName] = clientIP;
    logMessage("Client connected: " + deviceName + " (" + clientIP + ")");
    return true;
}

bool ClientManagement::removeClient(const std::string &deviceName) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = clients.find(deviceName);
    if (it == clients.end()) return false;
    
    if (it->second) it->second->close();
    clients.erase(it);
    clientIPs.erase(deviceName);
    return true;
}

bool ClientManagement::hasClient(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mtx);
    return clients.find(name) != clients.end();
}





Utils::TCPSocket* ClientManagement::getClient(const std::string &deviceName) const {
    std::lock_guard<std::mutex> lock(mtx);
    
    auto it = clients.find(deviceName);
    if (it == clients.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

std::vector<std::string> ClientManagement::listClients() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> names;
    for (auto &p : clients) {
        names.push_back(p.first);
    }
    return names;
}

std::string ClientManagement::getClientIP(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = clientIPs.find(name);
    return (it != clientIPs.end()) ? it->second : "unknown";
}





void ClientManagement::cleanup() {
    std::lock_guard<std::mutex> lock(mtx);
    
    
    for (auto &p : clients) {
        if (p.second) {
            p.second->close();
        }
    }
    clients.clear();
    clientIPs.clear();
}





std::string ClientManagement::generateUniqueDeviceName(const std::string& base, int& counter) const {
    
    
    std::string name = base;
    while (clients.find(name) != clients.end()) {
        name = base + "_" + std::to_string(++counter);
    }
    return name;
}

void ClientManagement::logMessage(const std::string &msg) {
    if (manager) {
        manager->pushLog(msg);
    }
}

} 
