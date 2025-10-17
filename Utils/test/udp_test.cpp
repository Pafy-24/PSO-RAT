#include <iostream>
#include "Utils/UDPSocket.hpp"
#include <SFML/Network/IpAddress.hpp>

int main() {
    Utils::UDPSocket server;
    if (!server.bind(54000)) {
        std::cerr << "Failed to bind server socket\n";
        return 1;
    }

    Utils::UDPSocket client;
    if (!client.bind(54001)) {
        std::cerr << "Failed to bind client socket\n";
        return 1;
    }

    sf::IpAddress localhost = sf::IpAddress::LocalHost;
    client.sendTo("ping", localhost, 54000);

    std::string msg;
    std::optional<sf::IpAddress> sender;
    unsigned short port;
    if (server.receive(msg, *sender, port)) {
        std::cout << "Server received from " << *sender << ":" << port << " -> " << msg << "\n";
    } else {
        std::cerr << "Server failed to receive\n";
        return 1;
    }

    return 0;
}
