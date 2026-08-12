#pragma once

#include <windows.h>

#include <optional>

#include "config/SettingsStore.h"

namespace wflow {

class SettingsWindow {
public:
    std::optional<AppSettings> ShowModal(HINSTANCE instance, HWND owner, const AppSettings& current);

private:
    static constexpr wchar_t kWindowClassName[] = L"WFlowSettingsWindow";

    static constexpr int kIdModelEdit = 2001;
    static constexpr int kIdBrowseButton = 2002;
    static constexpr int kIdLanguageEdit = 2003;
    static constexpr int kIdThreadsEdit = 2004;
    static constexpr int kIdStartupCheckbox = 2005;
    static constexpr int kIdClipboardCheckbox = 2006;
    static constexpr int kIdSaveButton = 2007;
    static constexpr int kIdCancelButton = 2008;
    static constexpr int kIdHotkeyLabel = 2009;
    static constexpr int kIdRecordHotkeyButton = 2010;
    static constexpr int kIdDownloadModelButton = 2011;
    static constexpr int kIdTestHotkeyButton = 2012;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool CreateWindowClass(HINSTANCE instance);
    bool CreateControls();
    void PopulateFromSettings();
    void UpdateHotkeyLabel();

    void OnBrowseModel();
    void OnRecordHotkey();
    void OnCaptureHotkey(UINT vkCode);
    void OnDownloadDefaultModel();
    void OnTestHotkey();
    bool OnSave();

    static bool IsModifierKey(UINT vkCode);
    static std::wstring HotkeyToString(const Hotkey& hotkey);

    HINSTANCE instance_ = nullptr;
    HWND owner_ = nullptr;
    HWND hwnd_ = nullptr;

    HWND modelEdit_ = nullptr;
    HWND languageEdit_ = nullptr;
    HWND threadsEdit_ = nullptr;
    HWND startupCheckbox_ = nullptr;
    HWND clipboardCheckbox_ = nullptr;
    HWND hotkeyLabel_ = nullptr;
    HWND recordHotkeyButton_ = nullptr;
    HWND downloadModelButton_ = nullptr;
    HWND testHotkeyButton_ = nullptr;

    AppSettings working_{};
    bool saved_ = false;
    bool recordingHotkey_ = false;
};

}  // namespace wflow
