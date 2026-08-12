#pragma once

#include <functional>
#include <string>

namespace wflow {

class ModelBootstrap {
public:
    using ProgressCallback = std::function<void(int)>;

    static bool DownloadDefaultModel(const std::wstring& targetPath,
                                     std::wstring& errorMessage,
                                     ProgressCallback progressCallback = {});
    static std::wstring DefaultModelUrl();
};

}  // namespace wflow
