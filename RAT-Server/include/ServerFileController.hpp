#pragma once

#include "IController.hpp"
#include <string>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

namespace Server {

class ServerManager;

class ServerFileController : public IController {
public:
    explicit ServerFileController(ServerManager *manager);
    ~ServerFileController();

    void start() override;
    void stop() override;
    
    std::string getHandle() const override { return "file"; }
    
    std::string handleDownload(const std::string &clientName, const std::string &remotePath, const std::string &localPath);
    std::string handleUpload(const std::string &clientName, const std::string &localPath, const std::string &remotePath);
    
    
    void handle(const nlohmann::json &packet) override;
    bool handleJson(const nlohmann::json &response) override;

private:
    ServerManager *manager;
    
    
    nlohmann::json pendingResponse;
    bool hasResponse = false;
    std::mutex responseMtx;
    std::condition_variable responseCv;
};

}
