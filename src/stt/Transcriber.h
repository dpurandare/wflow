#pragma once

#include <string>
#include <vector>

namespace wflow {

struct TranscriberConfig {
    std::wstring modelPath;
    std::string language = "en";
    int threads = 4;
};

class Transcriber {
public:
    Transcriber() = default;
    ~Transcriber();

    bool Initialize(const TranscriberConfig& config);
    std::wstring Transcribe(const std::vector<float>& samples);

    const std::wstring& LastError() const { return lastError_; }

private:
    TranscriberConfig config_;
    bool initialized_ = false;
    std::wstring lastError_;

#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    struct whisper_context* context_ = nullptr;
#endif
};

}  // namespace wflow
