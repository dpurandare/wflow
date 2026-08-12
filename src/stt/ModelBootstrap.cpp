#include "stt/ModelBootstrap.h"

#include <windows.h>
#include <wininet.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#pragma comment(lib, "wininet.lib")

namespace {

void CloseInternetHandle(HINTERNET& handle) {
    if (handle) {
        InternetCloseHandle(handle);
        handle = nullptr;
    }
}

}  // namespace

namespace wflow {

std::wstring ModelBootstrap::DefaultModelUrl() {
    return L"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin";
}

bool ModelBootstrap::DownloadDefaultModel(const std::wstring& targetPath,
                                          std::wstring& errorMessage,
                                          ProgressCallback progressCallback) {
    errorMessage.clear();

    if (targetPath.empty()) {
        errorMessage = L"Model path is empty.";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(targetPath).parent_path(), ec);

    const std::filesystem::path outputPath(targetPath);
    const std::filesystem::path tempPath = outputPath.wstring() + L".part";

    HINTERNET internet = InternetOpenW(L"wflow", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!internet) {
        errorMessage = L"Failed to initialize WinINet.";
        return false;
    }

    HINTERNET file = InternetOpenUrlW(
        internet, DefaultModelUrl().c_str(), nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!file) {
        CloseInternetHandle(internet);
        errorMessage = L"Failed to open model download URL.";
        return false;
    }

    DWORD contentLength = 0;
    DWORD lengthSize = sizeof(contentLength);
    const bool hasLength =
        HttpQueryInfoW(file, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &lengthSize, nullptr) == TRUE;

    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        CloseInternetHandle(file);
        CloseInternetHandle(internet);
        errorMessage = L"Failed to create output file for model download.";
        return false;
    }

    std::vector<char> buffer(64 * 1024);
    std::uint64_t totalRead = 0;
    int lastPercent = -1;

    while (true) {
        DWORD bytesRead = 0;
        if (!InternetReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead)) {
            out.close();
            std::error_code ignore;
            std::filesystem::remove(tempPath, ignore);
            CloseInternetHandle(file);
            CloseInternetHandle(internet);
            errorMessage = L"Download failed while reading network stream.";
            return false;
        }

        if (bytesRead == 0) {
            break;
        }

        out.write(buffer.data(), bytesRead);
        if (!out) {
            out.close();
            std::error_code ignore;
            std::filesystem::remove(tempPath, ignore);
            CloseInternetHandle(file);
            CloseInternetHandle(internet);
            errorMessage = L"Failed while writing model data to disk.";
            return false;
        }

        totalRead += bytesRead;
        if (progressCallback && hasLength && contentLength > 0) {
            const int percent = static_cast<int>(std::min<std::uint64_t>(100, (totalRead * 100) / contentLength));
            if (percent != lastPercent) {
                lastPercent = percent;
                progressCallback(percent);
            }
        }
    }

    out.close();
    CloseInternetHandle(file);
    CloseInternetHandle(internet);

    std::error_code renameError;
    std::filesystem::rename(tempPath, outputPath, renameError);
    if (renameError) {
        std::error_code copyError;
        std::filesystem::copy_file(tempPath, outputPath, std::filesystem::copy_options::overwrite_existing, copyError);
        std::error_code ignore;
        std::filesystem::remove(tempPath, ignore);
        if (copyError) {
            errorMessage = L"Model downloaded but failed to move file into final location.";
            return false;
        }
    }

    if (progressCallback) {
        progressCallback(100);
    }

    if (!std::filesystem::exists(targetPath)) {
        errorMessage = L"Download finished but model file was not found at the target path.";
        return false;
    }

    return true;
}

}  // namespace wflow
