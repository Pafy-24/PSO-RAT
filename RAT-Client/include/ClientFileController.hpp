#pragma once

#include "IClientController.hpp"

namespace Client {

class ClientFileController : public IClientController {
public:
    ClientFileController() = default;
    ~ClientFileController() override = default;

    nlohmann::json handleCommand(const nlohmann::json &cmd) override;
    std::string getHandle() const override { return "file"; }

private:
    nlohmann::json handleDownload(const std::string &path);
    nlohmann::json handleUpload(const std::string &path, const std::string &data);
};

}
