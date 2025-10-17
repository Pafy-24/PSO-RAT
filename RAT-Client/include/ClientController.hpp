#pragma once

#include "Utils/TCPSocket.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <string>

namespace Client {

class ClientController {
public:
    // Controller holds a shared pointer to the socket managed by ClientManager
    explicit ClientController(std::shared_ptr<Utils::TCPSocket> socket);
    ~ClientController();

    // non-copyable, movable
    ClientController(const ClientController &) = delete;
    ClientController &operator=(const ClientController &) = delete;
    ClientController(ClientController &&) = default;
    ClientController &operator=(ClientController &&) = default;

    // Returns whether controller has a valid socket
    bool isValid() const;

    // Send/receive JSON objects. Uses the string send/receive on Utils::TCPSocket.
    bool sendJson(const nlohmann::json &obj);
    bool receiveJson(nlohmann::json &out);

private:
    std::shared_ptr<Utils::TCPSocket> socket_;
    mutable std::mutex mtx_;
};

} // namespace Client
