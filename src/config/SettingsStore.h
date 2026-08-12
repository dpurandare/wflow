#pragma once

#include <string>

#include "platform/HotkeyManager.h"

namespace wflow {

struct AppSettings {
    Hotkey holdToTalkHotkey{};
    bool startWithWindows = false;
    bool enableClipboardFallback = false;
    std::wstring whisperModelPath;
    std::string whisperLanguage = "en";
    int whisperThreads = 4;
};

class SettingsStore {
public:
    AppSettings Load() const;
    bool Save(const AppSettings& settings) const;

private:
    std::wstring GetSettingsPath() const;
};

}  // namespace wflow
