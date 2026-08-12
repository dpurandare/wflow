#include "stt/Transcriber.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>

#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
#include <whisper.h>
#endif

namespace {

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return L"";
    }

    const int wideSize = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (wideSize <= 0) {
        return L"";
    }

    std::wstring wide(static_cast<size_t>(wideSize - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, wide.data(), wideSize);
    return wide;
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return "";
    }

    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0) {
        return "";
    }

    std::string utf8(static_cast<size_t>(utf8Size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, utf8.data(), utf8Size, nullptr, nullptr);
    return utf8;
}

std::wstring Trim(const std::wstring& input) {
    const auto begin = std::find_if_not(input.begin(), input.end(), [](wchar_t ch) {
        return ch == L' ' || ch == L'\n' || ch == L'\r' || ch == L'\t';
    });
    if (begin == input.end()) {
        return L"";
    }

    const auto end = std::find_if_not(input.rbegin(), input.rend(), [](wchar_t ch) {
        return ch == L' ' || ch == L'\n' || ch == L'\r' || ch == L'\t';
    }).base();

    return std::wstring(begin, end);
}

}  // namespace

namespace wflow {

Transcriber::~Transcriber() {
#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    if (context_) {
        whisper_free(context_);
        context_ = nullptr;
    }
#endif
}

bool Transcriber::Initialize(const TranscriberConfig& config) {
    config_ = config;
    lastError_.clear();

    if (config_.threads <= 0) {
        config_.threads = 4;
    }

#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    if (context_) {
        whisper_free(context_);
        context_ = nullptr;
    }

    if (config_.modelPath.empty()) {
        lastError_ = L"Whisper model path is empty.";
        return false;
    }

    if (!std::filesystem::exists(config_.modelPath)) {
        lastError_ = L"Whisper model file not found: " + config_.modelPath;
        return false;
    }

    const std::string modelPathUtf8 = WideToUtf8(config_.modelPath);
    context_ = whisper_init_from_file(modelPathUtf8.c_str());
    if (!context_) {
        lastError_ = L"Failed to load Whisper model.";
        return false;
    }
#endif

    initialized_ = true;
    return true;
}

std::wstring Transcriber::Transcribe(const std::vector<float>& samples) {
    if (!initialized_) {
        return L"";
    }

    lastError_.clear();

#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    if (!context_ || samples.empty()) {
        return L"";
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.translate = false;
    params.n_threads = config_.threads;
    params.language = config_.language.c_str();

    const int result = whisper_full(context_, params, samples.data(), static_cast<int>(samples.size()));
    if (result != 0) {
        return L"";
    }

    const int segmentCount = whisper_full_n_segments(context_);
    std::string utf8Transcript;
    for (int i = 0; i < segmentCount; ++i) {
        const char* segment = whisper_full_get_segment_text(context_, i);
        if (segment) {
            utf8Transcript += segment;
        }
    }

    return Trim(Utf8ToWide(utf8Transcript));
#else
    (void)config_;
    (void)samples;
    return L"";
#endif
}

}  // namespace wflow
