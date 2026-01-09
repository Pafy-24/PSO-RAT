#include "ClientFileController.hpp"
#include "Utils/Base64.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>

namespace Client {

static std::string expandPath(const std::string &path) {
    if (path.empty()) return path;
    
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home) {
            return std::string(home) + path.substr(1);
        }
    }
    return path;
}

nlohmann::json ClientFileController::handleCommand(const nlohmann::json &cmd) {
    nlohmann::json reply;
    reply["controller"] = "file";
    
    if (!cmd.contains("action")) {
        reply["success"] = false;
        reply["error"] = "Missing action field";
        return reply;
    }
    
    std::string action = cmd["action"];
    
    if (action == "download") {
        if (!cmd.contains("path")) {
            reply["success"] = false;
            reply["error"] = "Missing path field";
            return reply;
        }
        return handleDownload(cmd["path"]);
    } else if (action == "upload") {
        if (!cmd.contains("path") || !cmd.contains("data")) {
            reply["success"] = false;
            reply["error"] = "Missing path or data field";
            return reply;
        }
        return handleUpload(cmd["path"], cmd["data"]);
    } else {
        reply["success"] = false;
        reply["error"] = "Unknown action: " + action;
    }
    
    return reply;
}

nlohmann::json ClientFileController::handleDownload(const std::string &path) {
    nlohmann::json reply;
    reply["controller"] = "file";
    reply["action"] = "download";
    
    std::string expandedPath = expandPath(path);
    std::ifstream file(expandedPath, std::ios::binary);
    if (!file) {
        reply["success"] = false;
        reply["error"] = "Failed to open file: " + path;
        return reply;
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    if (content.size() > 50 * 1024 * 1024) {
        reply["success"] = false;
        reply["error"] = "File too large (max 50MB)";
        return reply;
    }
    
    reply["success"] = true;
    reply["data"] = Utils::base64_encode(content);
    reply["size"] = content.size();
    
    size_t lastSlash = path.find_last_of("/\\");
    reply["filename"] = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    
    return reply;
}

nlohmann::json ClientFileController::handleUpload(const std::string &path, const std::string &data) {
    nlohmann::json reply;
    reply["controller"] = "file";
    reply["action"] = "upload";
    
    try {
        std::string decoded = Utils::base64_decode(data);
        std::string expandedPath = expandPath(path);
        
        struct stat st;
        if (stat(expandedPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            reply["success"] = false;
            reply["error"] = "Path is a directory, please specify filename: " + path;
            return reply;
        }
        
        std::ofstream file(expandedPath, std::ios::binary);
        if (!file) {
            reply["success"] = false;
            reply["error"] = "Failed to create file: " + path;
            return reply;
        }
        
        file.write(decoded.data(), decoded.size());
        file.close();
        
        reply["success"] = true;
        reply["message"] = "File uploaded successfully";
        reply["size"] = decoded.size();
    } catch (const std::exception &e) {
        reply["success"] = false;
        reply["error"] = std::string("Exception: ") + e.what();
    }
    
    return reply;
}

}
