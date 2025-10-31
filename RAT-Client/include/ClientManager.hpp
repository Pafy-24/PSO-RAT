#pragma once

#include "Utils/TCPSocket.hpp"
#include "ClientController.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <string>

namespace Client {

class ClientManager {
public:
    static std::shared_ptr<ClientManager> getInstance();

    ClientManager(const ClientManager &) = delete;
    ClientManager &operator=(const ClientManager &) = delete;

    
    bool connectTo(const std::string &name, const sf::IpAddress &address, unsigned short port);
    
    void disconnect(const std::string &name);

    bool isConnected(const std::string &name) const;

    
    ClientController *getController(const std::string &name);

    
    
    int run(const sf::IpAddress &address, unsigned short port);

    ~ClientManager();

private:
    ClientManager();

    
    inline static std::shared_ptr<ClientManager> instance;
    
    
    inline static std::mutex initMutex;

    
    std::map<std::string, ClientController> controllers_;
    
    std::shared_ptr<Utils::TCPSocket> socket_;
    
    mutable std::mutex mtx_;
};

} 
