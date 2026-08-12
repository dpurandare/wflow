#include "platform/StartupManager.h"

#include <windows.h>

#include <string>

namespace {

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"wflow";

std::wstring CurrentExecutablePath() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0) {
        return L"";
    }

    while (length == path.size()) {
        path.resize(path.size() * 2);
        length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            return L"";
        }
    }

    path.resize(length);
    return path;
}

}  // namespace

namespace wflow {

bool StartupManager::IsEnabled() const {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    const LONG result = RegGetValueW(key, nullptr, kRunValueName, RRF_RT_REG_SZ, &type, nullptr, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ && size > sizeof(wchar_t);
}

bool StartupManager::SetEnabled(bool enabled) const {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        kRunKeyPath,
                        0,
                        nullptr,
                        REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE,
                        nullptr,
                        &key,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }

    bool ok = false;
    if (enabled) {
        const std::wstring executable = CurrentExecutablePath();
        if (!executable.empty()) {
            const std::wstring quoted = L"\"" + executable + L"\"";
            const BYTE* data = reinterpret_cast<const BYTE*>(quoted.c_str());
            const DWORD bytes = static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t));
            ok = RegSetValueExW(key, kRunValueName, 0, REG_SZ, data, bytes) == ERROR_SUCCESS;
        }
    } else {
        const LONG result = RegDeleteValueW(key, kRunValueName);
        ok = (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(key);
    return ok;
}

}  // namespace wflow
