#pragma once

#include "Utils/TCPSocket.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <string>

namespace Client {

class ClientController {
public:
    
    explicit ClientController(std::shared_ptr<Utils::TCPSocket> socket);
    ~ClientController();

    
    ClientController(const ClientController &) = delete;
    ClientController &operator=(const ClientController &) = delete;
    ClientController(ClientController &&) = default;
    ClientController &operator=(ClientController &&) = default;

    
    bool isValid() const;

    
    bool sendJson(const nlohmann::json &obj);
    bool receiveJson(nlohmann::json &out);

private:
    std::shared_ptr<Utils::TCPSocket> socket_;
    mutable std::mutex mtx_;
};

} 
