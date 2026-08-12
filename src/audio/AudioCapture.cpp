#include "audio/AudioCapture.h"

#include <audioclient.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace {

template <typename T>
void SafeRelease(T*& pointer) {
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}

float ClampSample(float value) {
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

constexpr double kPi = 3.14159265358979323846;

// Windowed-sinc low-pass kernel (Hamming window), normalizedCutoff = cutoffHz / sampleRate.
std::vector<float> BuildLowPassKernel(double normalizedCutoff, int numTaps) {
    std::vector<float> kernel(static_cast<size_t>(numTaps));
    const double center = (numTaps - 1) / 2.0;
    double sum = 0.0;

    for (int n = 0; n < numTaps; ++n) {
        const double x = n - center;
        const double sinc = (x == 0.0) ? 2.0 * normalizedCutoff
                                        : std::sin(2.0 * kPi * normalizedCutoff * x) / (kPi * x);
        const double window = 0.54 - 0.46 * std::cos(2.0 * kPi * n / (numTaps - 1));
        const double value = sinc * window;
        kernel[static_cast<size_t>(n)] = static_cast<float>(value);
        sum += value;
    }

    if (sum != 0.0) {
        for (float& tap : kernel) {
            tap = static_cast<float>(tap / sum);
        }
    }

    return kernel;
}

std::vector<float> ApplyFirFilter(const std::vector<float>& input, const std::vector<float>& kernel) {
    const int numTaps = static_cast<int>(kernel.size());
    const int half = numTaps / 2;
    std::vector<float> output(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        double acc = 0.0;
        for (int k = 0; k < numTaps; ++k) {
            const long long idx = static_cast<long long>(i) + k - half;
            if (idx >= 0 && idx < static_cast<long long>(input.size())) {
                acc += static_cast<double>(input[static_cast<size_t>(idx)]) * kernel[static_cast<size_t>(k)];
            }
        }
        output[i] = static_cast<float>(acc);
    }

    return output;
}

void AppendSamplesFromBuffer(const BYTE* data,
                             UINT32 frameCount,
                             int channels,
                             bool isFloat,
                             bool is16BitPcm,
                             bool silent,
                             std::vector<float>& output) {
    if (channels <= 0 || frameCount == 0) {
        return;
    }

    const size_t baseSize = output.size();
    output.resize(baseSize + frameCount);

    if (silent || !data) {
        std::fill(output.begin() + static_cast<std::ptrdiff_t>(baseSize), output.end(), 0.0f);
        return;
    }

    for (UINT32 frame = 0; frame < frameCount; ++frame) {
        float mono = 0.0f;

        if (isFloat) {
            const float* samples = reinterpret_cast<const float*>(data);
            for (int channel = 0; channel < channels; ++channel) {
                mono += samples[frame * static_cast<UINT32>(channels) + static_cast<UINT32>(channel)];
            }
        } else if (is16BitPcm) {
            const int16_t* samples = reinterpret_cast<const int16_t*>(data);
            for (int channel = 0; channel < channels; ++channel) {
                const int16_t sample = samples[frame * static_cast<UINT32>(channels) + static_cast<UINT32>(channel)];
                mono += static_cast<float>(sample) / 32768.0f;
            }
        }

        mono /= static_cast<float>(channels);
        output[baseSize + frame] = ClampSample(mono);
    }
}

}  // namespace

namespace wflow {

AudioCapture::~AudioCapture() {
    (void)Stop();
}

bool AudioCapture::Start() {
    if (capturing_.exchange(true)) {
        return false;
    }

    stopRequested_ = false;

    {
        std::lock_guard<std::mutex> lock(samplesMutex_);
        rawSamples_.clear();
    }

    captureThread_ = std::thread(&AudioCapture::CaptureLoop, this);
    return true;
}

std::vector<float> AudioCapture::Stop() {
    if (!capturing_.exchange(false)) {
        return {};
    }

    stopRequested_ = true;

    if (captureThread_.joinable()) {
        captureThread_.join();
    }

    std::vector<float> samples;
    {
        std::lock_guard<std::mutex> lock(samplesMutex_);
        samples = rawSamples_;
        rawSamples_.clear();
    }

    return ResampleTo16k(samples, captureSampleRate_);
}

std::vector<float> AudioCapture::ResampleTo16k(const std::vector<float>& input, int inputSampleRate) {
    if (input.empty() || inputSampleRate <= 0) {
        return {};
    }

    constexpr int kTargetRate = 16000;
    if (inputSampleRate == kTargetRate) {
        return input;
    }

    const double ratio = static_cast<double>(inputSampleRate) / static_cast<double>(kTargetRate);

    std::vector<float> filtered;
    const std::vector<float>* source = &input;
    if (ratio > 1.0) {
        // Downsampling: low-pass below the target Nyquist first, or high-frequency
        // content aliases into the speech band and badly corrupts transcription.
        constexpr int kNumTaps = 127;
        const double cutoffHz = static_cast<double>(kTargetRate) * 0.45;
        const double normalizedCutoff = cutoffHz / static_cast<double>(inputSampleRate);
        const std::vector<float> kernel = BuildLowPassKernel(normalizedCutoff, kNumTaps);
        filtered = ApplyFirFilter(input, kernel);
        source = &filtered;
    }

    const size_t outputCount = static_cast<size_t>(std::max(1.0, std::floor(source->size() / ratio)));

    std::vector<float> output;
    output.reserve(outputCount);

    for (size_t i = 0; i < outputCount; ++i) {
        const double sourceIndex = static_cast<double>(i) * ratio;
        const size_t left = static_cast<size_t>(sourceIndex);
        const size_t right = std::min(left + 1, source->size() - 1);
        const double fraction = sourceIndex - static_cast<double>(left);

        const float sample = static_cast<float>((1.0 - fraction) * (*source)[left] + fraction * (*source)[right]);
        output.push_back(sample);
    }

    return output;
}

void AudioCapture::CaptureLoop() {
    const HRESULT comInitResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT result = comInitResult;
    const bool comReady = SUCCEEDED(comInitResult) || comInitResult == RPC_E_CHANGED_MODE;

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioCaptureClient* captureClient = nullptr;
    WAVEFORMATEX* mixFormat = nullptr;

    if (!comReady) {
        stopRequested_ = true;
        return;
    }

    result = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (FAILED(result)) {
        stopRequested_ = true;
        goto cleanup;
    }

    result = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
    if (FAILED(result)) {
        stopRequested_ = true;
        goto cleanup;
    }

    result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient));
    if (FAILED(result)) {
        stopRequested_ = true;
        goto cleanup;
    }

    result = audioClient->GetMixFormat(&mixFormat);
    if (FAILED(result) || !mixFormat) {
        stopRequested_ = true;
        goto cleanup;
    }

    captureSampleRate_ = static_cast<int>(mixFormat->nSamplesPerSec);

    result = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                     0,
                                     500000,
                                     0,
                                     mixFormat,
                                     nullptr);
    if (FAILED(result)) {
        stopRequested_ = true;
        goto cleanup;
    }

    result = audioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&captureClient));
    if (FAILED(result)) {
        stopRequested_ = true;
        goto cleanup;
    }

    result = audioClient->Start();
    if (FAILED(result)) {
        stopRequested_ = true;
        goto cleanup;
    }

    while (!stopRequested_) {
        Sleep(10);

        UINT32 packetFrames = 0;
        if (FAILED(captureClient->GetNextPacketSize(&packetFrames))) {
            break;
        }

        while (packetFrames > 0) {
            BYTE* data = nullptr;
            UINT32 frameCount = 0;
            DWORD flags = 0;

            if (FAILED(captureClient->GetBuffer(&data, &frameCount, &flags, nullptr, nullptr))) {
                packetFrames = 0;
                break;
            }

            const bool isSilent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

            bool isFloat = false;
            bool is16BitPcm = false;

            if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT && mixFormat->wBitsPerSample == 32) {
                isFloat = true;
            } else if (mixFormat->wFormatTag == WAVE_FORMAT_PCM && mixFormat->wBitsPerSample == 16) {
                is16BitPcm = true;
            } else if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
                const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mixFormat);
                if (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && mixFormat->wBitsPerSample == 32) {
                    isFloat = true;
                } else if (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && mixFormat->wBitsPerSample == 16) {
                    is16BitPcm = true;
                }
            }

            if (isFloat || is16BitPcm) {
                std::lock_guard<std::mutex> lock(samplesMutex_);
                AppendSamplesFromBuffer(data,
                                        frameCount,
                                        static_cast<int>(mixFormat->nChannels),
                                        isFloat,
                                        is16BitPcm,
                                        isSilent,
                                        rawSamples_);
            }

            captureClient->ReleaseBuffer(frameCount);

            if (FAILED(captureClient->GetNextPacketSize(&packetFrames))) {
                packetFrames = 0;
            }
        }
    }

    (void)audioClient->Stop();

cleanup:
    if (mixFormat) {
        CoTaskMemFree(mixFormat);
        mixFormat = nullptr;
    }
    SafeRelease(captureClient);
    SafeRelease(audioClient);
    SafeRelease(device);
    SafeRelease(enumerator);

    if (SUCCEEDED(comInitResult)) {
        CoUninitialize();
    }
}

}  // namespace wflow
