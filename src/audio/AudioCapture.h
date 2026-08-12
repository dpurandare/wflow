#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace wflow {

class AudioCapture {
public:
    AudioCapture() = default;
    ~AudioCapture();

    bool Start();
    std::vector<float> Stop();
    bool IsCapturing() const { return capturing_; }

private:
    static std::vector<float> ResampleTo16k(const std::vector<float>& input, int inputSampleRate);

    void CaptureLoop();

    std::atomic<bool> capturing_ = false;
    std::atomic<bool> stopRequested_ = false;
    std::thread captureThread_;

    std::mutex samplesMutex_;
    std::vector<float> rawSamples_;
    int captureSampleRate_ = 16000;
};

}  // namespace wflow
