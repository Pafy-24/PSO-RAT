#include "ClientManager.hpp"
#include <iostream>
#include <arpa/inet.h>

int main() {
    const char *server_ip = "127.0.0.1";
    const unsigned short port = 5555;
    // construct sf::IpAddress from numeric representation
    in_addr_t a = inet_addr(server_ip);
    uint32_t hostOrder = ntohl(a);
    sf::IpAddress ip(hostOrder);
    int rc = Client::ClientManager::getInstance()->run(ip, port);
    if (rc != 0) std::cerr << "Client run failed: " << rc << std::endl;
    return rc;
}
