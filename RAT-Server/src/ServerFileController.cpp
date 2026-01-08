#include "ServerFileController.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <sys/stat.h>
#include <pwd.h>
#include <cstdlib>

namespace Server {

static std::string expandPath(const std::string &path) {
    if (path.empty() || path[0] != '~') return path;
    
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : nullptr;
    }
    
    if (!home) return path;
    
    if (path.length() == 1 || path[1] == '/') {
        return std::string(home) + path.substr(1);
    }
    return path;
}

ServerFileController::ServerFileController(ServerManager *manager) : IController(manager), manager_(manager) {}
ServerFileController::~ServerFileController() = default;

void ServerFileController::start() {}
void ServerFileController::stop() {}

void ServerFileController::handle(const nlohmann::json &packet) {
    
    handleJson(packet);
}

bool ServerFileController::handleJson(const nlohmann::json &response) {
    std::lock_guard<std::mutex> lock(responseMtx_);
    pendingResponse_ = response;
    hasResponse_ = true;
    responseCv_.notify_one();
    return true;
}

std::string ServerFileController::handleDownload(const std::string &clientName, const std::string &remotePath, const std::string &localPath) {
    if (!manager_ || clientName.empty() || !manager_->hasClient(clientName)) return "Error\n";
    if (remotePath.empty() || localPath.empty()) return "Usage: download [client] <remote_path> <local_path>\n";
    
    nlohmann::json cmd;
    cmd["controller"] = "file";
    cmd["action"] = "download";
    cmd["path"] = remotePath;
    
    if (!manager_->sendRequest(clientName, cmd)) return "Failed\n";
    
    struct termios oldSettings, newSettings;
    tcgetattr(STDIN_FILENO, &oldSettings);
    newSettings = oldSettings;
    newSettings.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
    int oldFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldFlags | O_NONBLOCK);
    
    nlohmann::json response;
    bool received = false;
    bool cancelled = false;
    int dots = 0;
    
    auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    
    while (!received && !cancelled) {
        
        if (manager_->receiveResponse(clientName, response, 100)) {
            received = true;
            break;
        }
        
        char ch;
        if (read(STDIN_FILENO, &ch, 1) == 1 && (ch == 'q' || ch == 'Q')) {
            cancelled = true;
            break;
        }
        
        if (std::chrono::steady_clock::now() >= endTime) {
            fcntl(STDIN_FILENO, F_SETFL, oldFlags);
            tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
            return "";
        }
        
        if (++dots % 10 == 0) {
            std::cout << "." << std::flush;
        }
    }
    
    fcntl(STDIN_FILENO, F_SETFL, oldFlags);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
    
    if (cancelled) return "";
    if (!response.contains("success") || !response["success"].get<bool>()) return "Failed\n";
    if (!response.contains("data")) return "Error\n";
    
    std::string data = response["data"].get<std::string>();
    std::string decoded;
    
    static const char decode_table[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };
    
    int val = 0, valb = -8;
    for (unsigned char c : data) {
        if (decode_table[c] == -1) break;
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    std::string actualPath = expandPath(localPath);
    
    
    struct stat st;
    if (stat(actualPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        std::string filename = response.contains("filename") ? response["filename"].get<std::string>() : "downloaded_file";
        if (actualPath.back() != '/') actualPath += '/';
        actualPath += filename;
    } else if (localPath == "." || localPath.back() == '/') {
        std::string filename = response.contains("filename") ? response["filename"].get<std::string>() : "downloaded_file";
        actualPath = (localPath == ".") ? filename : actualPath + filename;
    }
    
    std::ofstream outFile(actualPath, std::ios::binary);
    if (!outFile) return "Failed to create local file: " + actualPath + "\n";
    
    outFile.write(decoded.data(), decoded.size());
    outFile.close();
    
    size_t fileSize = response.contains("size") ? response["size"].get<size_t>() : decoded.size();
    return "Downloaded " + std::to_string(fileSize) + " bytes to " + actualPath + "\n";
}

std::string ServerFileController::handleUpload(const std::string &clientName, const std::string &localPath, const std::string &remotePath) {
    if (!manager_) return "Manager not available\n";
    
    if (clientName.empty()) return "No client specified\n";
    if (!manager_->hasClient(clientName)) return "Client not found\n";
    
    if (localPath.empty()) return "Usage: upload [client] <local_path> <remote_path>\n";
    if (remotePath.empty()) return "Usage: upload [client] <local_path> <remote_path>\n";
    
    std::ifstream file(localPath, std::ios::binary);
    if (!file) return "Failed to open local file: " + localPath + "\n";
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    if (content.size() > 50 * 1024 * 1024) {
        return "File too large (max 50MB)\n";
    }
    
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string encoded;
    int val = 0, valb = -6;
    for (unsigned char c : content) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (encoded.size() % 4) encoded.push_back('=');
    
    std::string actualRemotePath = remotePath;
    if (remotePath == "." || remotePath.back() == '/' || remotePath.find('.') == std::string::npos) {
        size_t lastSlash = localPath.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ? localPath.substr(lastSlash + 1) : localPath;
        if (remotePath == ".") {
            actualRemotePath = filename;
        } else if (remotePath.back() == '/') {
            actualRemotePath = remotePath + filename;
        } else {
            actualRemotePath = remotePath + "/" + filename;
        }
    }
    
    nlohmann::json cmd;
    cmd["controller"] = "file";
    cmd["action"] = "upload";
    cmd["path"] = actualRemotePath;
    cmd["data"] = encoded;
    
    if (!manager_->sendRequest(clientName, cmd)) return "Failed\n";
    
    struct termios oldSettings, newSettings;
    tcgetattr(STDIN_FILENO, &oldSettings);
    newSettings = oldSettings;
    newSettings.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
    int oldFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldFlags | O_NONBLOCK);
    
    nlohmann::json response;
    bool received = false;
    bool cancelled = false;
    int dots = 0;
    
    auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    
    while (!received && !cancelled) {
        
        if (manager_->receiveResponse(clientName, response, 100)) {
            received = true;
            break;
        }
        
        char ch;
        if (read(STDIN_FILENO, &ch, 1) == 1 && (ch == 'q' || ch == 'Q')) {
            cancelled = true;
            break;
        }
        
        if (std::chrono::steady_clock::now() >= endTime) {
            fcntl(STDIN_FILENO, F_SETFL, oldFlags);
            tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
            return "";
        }
        
        if (++dots % 10 == 0) {
            std::cout << "." << std::flush;
        }
    }
    
    fcntl(STDIN_FILENO, F_SETFL, oldFlags);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
    
    if (cancelled) return "";
    if (!response.contains("success") || !response["success"].get<bool>()) return "Failed\n";
    
    return "OK\n";
}

}
