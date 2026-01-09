#pragma once

#include "Utils/TCPSocket.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <string>
#include <vector>

namespace Server {

class ServerManager;

/**
 * ClientManagement - Manages connected clients and their controllers
 * 
 * Responsibilities:
 * - Client socket registry (deviceName -> socket)
 * - Client IP address tracking
 * - Per-client controller lifecycle management
 * - Client queries and lookup operations
 */
class ClientManagement {
public:
    explicit ClientManagement(ServerManager* manager);
    ~ClientManagement();

    
    
    
    bool addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket);
    bool removeClient(const std::string &deviceName);
    bool hasClient(const std::string &name) const;
    
    
    
    
    Utils::TCPSocket* getClient(const std::string &deviceName) const;
    std::vector<std::string> listClients() const;
    std::string getClientIP(const std::string &name) const;
    
    
    
    
    void cleanup();
    
    
    
    
    std::string generateUniqueDeviceName(const std::string& base, int& counter) const;

private:
    ServerManager* manager;
    mutable std::mutex mtx;
    
    
    std::map<std::string, std::unique_ptr<Utils::TCPSocket>> clients;
    std::map<std::string, std::string> clientIPs;
    
    void logMessage(const std::string &msg);
};

} 
