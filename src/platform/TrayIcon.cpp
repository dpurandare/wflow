#include "platform/TrayIcon.h"

#include <shellapi.h>

#include "app/resource.h"

namespace wflow {

bool TrayIcon::Create(HWND hwnd, UINT callbackMessage) {
    HINSTANCE moduleInstance = GetModuleHandleW(nullptr);
    idleIcon_ = LoadIconW(moduleInstance, MAKEINTRESOURCE(IDI_TRAY_IDLE));
    listeningIcon_ = LoadIconW(moduleInstance, MAKEINTRESOURCE(IDI_TRAY_LISTENING));

    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
    nid_.uCallbackMessage = callbackMessage;
    nid_.hIcon = idleIcon_ ? idleIcon_ : LoadIconW(nullptr, IDI_APPLICATION);

    wcscpy_s(nid_.szTip, L"wflow");

    created_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    if (!created_) {
        return false;
    }

    RefreshIcon();
    return true;
}

void TrayIcon::Destroy() {
    if (!created_) {
        return;
    }

    Shell_NotifyIconW(NIM_DELETE, &nid_);
    created_ = false;

    if (idleIcon_) {
        DestroyIcon(idleIcon_);
        idleIcon_ = nullptr;
    }
    if (listeningIcon_) {
        DestroyIcon(listeningIcon_);
        listeningIcon_ = nullptr;
    }
    nid_.hIcon = nullptr;
}

void TrayIcon::SetStatusText(const std::wstring& status) {
    statusText_ = status;
    RefreshIcon();
}

void TrayIcon::SetHotkeyText(const std::wstring& hotkey) {
    hotkeyText_ = hotkey;
}

void TrayIcon::SetListening(bool isListening) {
    listening_ = isListening;
    RefreshIcon();
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& message) {
    if (!created_) {
        return;
    }

    NOTIFYICONDATAW balloon = nid_;
    balloon.uFlags = NIF_INFO;
    balloon.dwInfoFlags = NIIF_INFO;
    balloon.uTimeout = 4000;
    wcsncpy_s(balloon.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(balloon.szInfo, message.c_str(), _TRUNCATE);

    Shell_NotifyIconW(NIM_MODIFY, &balloon);
}

void TrayIcon::ShowContextMenu(HWND owner, POINT cursorPosition) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    std::wstring hotkeyLine = L"Hotkey: " + hotkeyText_;
    std::wstring statusLine = L"Status: " + statusText_;

    MENUITEMINFOW hotkeyItem{};
    hotkeyItem.cbSize = sizeof(hotkeyItem);
    hotkeyItem.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
    hotkeyItem.wID = 0;
    hotkeyItem.fState = MFS_DISABLED;
    hotkeyItem.dwTypeData = hotkeyLine.data();
    InsertMenuItemW(menu, 0, TRUE, &hotkeyItem);

    MENUITEMINFOW statusItem{};
    statusItem.cbSize = sizeof(statusItem);
    statusItem.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
    statusItem.wID = 0;
    statusItem.fState = MFS_DISABLED;
    statusItem.dwTypeData = statusLine.data();
    InsertMenuItemW(menu, 1, TRUE, &statusItem);

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandToggleListening, L"Toggle listening");
    AppendMenuW(menu, MF_STRING, kCommandDownloadModel, L"Download Default Model");
    AppendMenuW(menu, MF_STRING, kCommandSettings, L"Settings...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandQuit, L"Quit");

    SetForegroundWindow(owner);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursorPosition.x, cursorPosition.y, 0, owner, nullptr);
    DestroyMenu(menu);
}

void TrayIcon::RefreshIcon() {
    if (!created_) {
        return;
    }

    std::wstring tooltip = L"wflow - " + statusText_;
    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);
    nid_.hIcon = (listening_ && listeningIcon_) ? listeningIcon_ : idleIcon_;

    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

}  // namespace wflow
