#include "platform/HotkeyManager.h"

namespace wflow {

HotkeyManager* HotkeyManager::instance_ = nullptr;

bool HotkeyManager::Start(HWND hwnd, UINT id, Hotkey hotkey, Callback onPress, Callback onRelease) {
    Stop();

    hwnd_ = hwnd;
    id_ = id;
    hotkey_ = hotkey;
    onPress_ = std::move(onPress);
    onRelease_ = std::move(onRelease);

    if (!RegisterHotKey(hwnd_, id_, hotkey_.modifiers | MOD_NOREPEAT, hotkey_.vk)) {
        return false;
    }

    return InstallHook();
}

void HotkeyManager::Stop() {
    if (hwnd_ && id_ != 0) {
        UnregisterHotKey(hwnd_, id_);
    }

    RemoveHook();

    hwnd_ = nullptr;
    id_ = 0;
    hotkeyActive_ = false;
}

void HotkeyManager::HandleWindowMessage(UINT message, WPARAM wParam, LPARAM) {
    if (message != WM_HOTKEY || static_cast<UINT>(wParam) != id_) {
        return;
    }

    if (hotkeyActive_) {
        return;
    }

    hotkeyActive_ = true;
    if (onPress_) {
        onPress_();
    }
}

LRESULT CALLBACK HotkeyManager::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || !instance_) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    const auto* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (!key) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && instance_->hotkeyActive_ && key->vkCode == instance_->hotkey_.vk) {
        instance_->hotkeyActive_ = false;
        if (instance_->onRelease_) {
            instance_->onRelease_();
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool HotkeyManager::InstallHook() {
    instance_ = this;
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyManager::KeyboardProc, GetModuleHandleW(nullptr), 0);
    return hook_ != nullptr;
}

void HotkeyManager::RemoveHook() {
    if (hook_) {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }

    if (instance_ == this) {
        instance_ = nullptr;
    }
}

bool HotkeyManager::RequiredModifiersDown() const {
    bool ctrlOk = !(hotkey_.modifiers & MOD_CONTROL) || (GetAsyncKeyState(VK_CONTROL) & 0x8000);
    bool altOk = !(hotkey_.modifiers & MOD_ALT) || (GetAsyncKeyState(VK_MENU) & 0x8000);
    bool shiftOk = !(hotkey_.modifiers & MOD_SHIFT) || (GetAsyncKeyState(VK_SHIFT) & 0x8000);
    bool winOk = !(hotkey_.modifiers & MOD_WIN) || ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000));
    return ctrlOk && altOk && shiftOk && winOk;
}

}  // namespace wflow
