#pragma once

namespace Utils {

class Socket {
public:
    virtual ~Socket() = default;

    
    virtual bool bind(unsigned short port) = 0;

    
    virtual void close() = 0;
};

} 
