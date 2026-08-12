#include "config/SettingsStore.h"

#include <ShlObj.h>

#include <filesystem>
#include <fstream>
#include <thread>

namespace {

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

std::wstring RoamingWflowDirectory() {
    wchar_t* roamingPathRaw = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingPathRaw) != S_OK) {
        return L".";
    }

    std::wstring roamingPath(roamingPathRaw);
    CoTaskMemFree(roamingPathRaw);

    return (std::filesystem::path(roamingPath) / L"wflow").wstring();
}

std::wstring DefaultModelPath() {
    const std::filesystem::path baseDir = RoamingWflowDirectory();
    return (baseDir / L"models" / L"ggml-base.en.bin").wstring();
}

std::wstring NormalizeModelPath(const std::wstring& configuredPath) {
    if (configuredPath.empty()) {
        return DefaultModelPath();
    }

    const std::filesystem::path path(configuredPath);
    if (!path.is_absolute()) {
        const std::filesystem::path fileName = path.filename().empty() ? L"ggml-base.en.bin" : path.filename();
        return (std::filesystem::path(RoamingWflowDirectory()) / L"models" / fileName).wstring();
    }

    const std::wstring pathText = path.wstring();
    if (pathText.find(L"\\WindowsApps\\") != std::wstring::npos) {
        const std::filesystem::path fileName = path.filename().empty() ? L"ggml-base.en.bin" : path.filename();
        return (std::filesystem::path(RoamingWflowDirectory()) / L"models" / fileName).wstring();
    }

    return path.wstring();
}

}  // namespace

namespace wflow {

AppSettings SettingsStore::Load() const {
    AppSettings settings;

    std::ifstream file(GetSettingsPath());
    if (file) {
        std::string line;
        while (std::getline(file, line)) {
            const auto equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }

            const std::string key = line.substr(0, equals);
            const std::string value = line.substr(equals + 1);

            if (key == "hotkey_modifiers") {
                settings.holdToTalkHotkey.modifiers = static_cast<UINT>(std::stoul(value));
            } else if (key == "hotkey_vk") {
                settings.holdToTalkHotkey.vk = static_cast<UINT>(std::stoul(value));
            } else if (key == "start_with_windows") {
                settings.startWithWindows = (value == "1");
            } else if (key == "clipboard_fallback") {
                settings.enableClipboardFallback = (value == "1");
            } else if (key == "whisper_model_path") {
                settings.whisperModelPath = Utf8ToWide(value);
            } else if (key == "whisper_language") {
                settings.whisperLanguage = value.empty() ? "en" : value;
            } else if (key == "whisper_threads") {
                const int parsed = atoi(value.c_str());
                if (parsed > 0) {
                    settings.whisperThreads = parsed;
                }
            }
        }
    }

    settings.whisperModelPath = NormalizeModelPath(settings.whisperModelPath);

    if (settings.whisperThreads <= 0) {
        const unsigned cpu = std::thread::hardware_concurrency();
        settings.whisperThreads = static_cast<int>(cpu == 0 ? 4 : std::min(8u, cpu));
    }

    return settings;
}

bool SettingsStore::Save(const AppSettings& settings) const {
    const std::wstring path = GetSettingsPath();
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }

    file << "hotkey_modifiers=" << settings.holdToTalkHotkey.modifiers << "\n";
    file << "hotkey_vk=" << settings.holdToTalkHotkey.vk << "\n";
    file << "start_with_windows=" << (settings.startWithWindows ? "1" : "0") << "\n";
    file << "clipboard_fallback=" << (settings.enableClipboardFallback ? "1" : "0") << "\n";
    file << "whisper_model_path=" << WideToUtf8(settings.whisperModelPath) << "\n";
    file << "whisper_language=" << settings.whisperLanguage << "\n";
    file << "whisper_threads=" << settings.whisperThreads << "\n";

    return true;
}

std::wstring SettingsStore::GetSettingsPath() const {
    wchar_t* roamingPathRaw = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingPathRaw) != S_OK) {
        return L"settings.ini";
    }

    std::wstring roamingPath(roamingPathRaw);
    CoTaskMemFree(roamingPathRaw);

    std::filesystem::path configPath = std::filesystem::path(roamingPath) / L"wflow" / L"settings.ini";
    return configPath.wstring();
}

}  // namespace wflow
