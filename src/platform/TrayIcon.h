#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace wflow {

class TrayIcon {
public:
    static constexpr UINT kCommandToggleListening = 1001;
    static constexpr UINT kCommandSettings = 1002;
    static constexpr UINT kCommandQuit = 1003;
    static constexpr UINT kCommandDownloadModel = 1004;

    bool Create(HWND hwnd, UINT callbackMessage);
    void Destroy();

    void SetStatusText(const std::wstring& status);
    void SetHotkeyText(const std::wstring& hotkey);
    void SetListening(bool isListening);
    void ShowBalloon(const std::wstring& title, const std::wstring& message);
    void ShowContextMenu(HWND owner, POINT cursorPosition);

private:
    void RefreshIcon();

    NOTIFYICONDATAW nid_{};
    std::wstring statusText_ = L"Initializing";
    std::wstring hotkeyText_ = L"Ctrl + Alt + Z";
    bool created_ = false;
    bool listening_ = false;
    HICON idleIcon_ = nullptr;
    HICON listeningIcon_ = nullptr;
};

}  // namespace wflow
