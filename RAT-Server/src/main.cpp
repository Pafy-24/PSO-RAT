#include "ServerManager.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char** argv) {
    unsigned short port = 5555;
    
    // Allow port override from command line
    if (argc > 1) {
        int p = std::atoi(argv[1]);
        if (p > 0 && p <= 65535) {
            port = static_cast<unsigned short>(p);
        } else {
            std::cerr << "Invalid port number. Using default: 5555" << std::endl;
        }
    }
    
    int rc = Server::ServerManager::getInstance()->run(port);
    if (rc != 0) std::cerr << "Server run failed: " << rc << std::endl;
    return rc;
}
