#include "ServerManager.hpp"
#include <iostream>

int main() {
    const unsigned short port = 5555;
    int rc = Server::ServerManager::getInstance()->run(port);
    if (rc != 0) std::cerr << "Server run failed: " << rc << std::endl;
    return rc;
}
