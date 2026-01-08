#pragma once

#include "IClientController.hpp"
#include "Utils/TCPSocket.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <string>

namespace Client {

class ClientController : public IClientController {
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
    
    nlohmann::json handleCommand(const nlohmann::json &request) override;
    std::string getHandle() const override { return "tcp"; }

private:
    std::shared_ptr<Utils::TCPSocket> socket_;
    mutable std::mutex mtx_;
};

} 
