#include "ClientBashController.hpp"
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/wait.h>

namespace Client {

nlohmann::json ClientBashController::handleCommand(const nlohmann::json &request) {
    nlohmann::json reply;
    reply["controller"] = "bash";
    
    if (!request.contains("cmd") || !request["cmd"].is_string()) {
        reply["out"] = "Invalid request: missing 'cmd' field";
        reply["ret"] = -1;
        return reply;
    }
    
    std::string cmd = request["cmd"].get<std::string>();
    
    if (cmd == "exit") {
        reply["out"] = "Exiting";
        reply["ret"] = 0;
        reply["exit"] = true;
        return reply;
    }
    
    if (cmd.empty()) {
        reply["out"] = "Empty command";
        reply["ret"] = -1;
        return reply;
    }
    
    std::string output;
    int retcode = 0;
    FILE *p = popen(cmd.c_str(), "r");
    
    if (!p) {
        output = std::string("popen failed: ") + std::strerror(errno);
        retcode = -1;
    } else {
        char buf[512];
        while (fgets(buf, sizeof(buf), p) != nullptr) {
            output += buf;
            if (output.size() > 1024 * 1024) {
                output += "\n[Output truncated - too large]";
                break;
            }
        }
        
        int status = pclose(p);
        if (status == -1) {
            retcode = -1;
        } else if (WIFEXITED(status)) {
            retcode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            retcode = -WTERMSIG(status);
        } else {
            retcode = status;
        }
    }
    
    reply["out"] = output;
    reply["ret"] = retcode;
    return reply;
}

}
