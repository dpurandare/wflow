#pragma once

#include <windows.h>

#include <functional>

namespace wflow {

struct Hotkey {
    UINT modifiers = MOD_CONTROL | MOD_ALT;
    UINT vk = 'Z';
};

class HotkeyManager {
public:
    using Callback = std::function<void()>;

    bool Start(HWND hwnd, UINT id, Hotkey hotkey, Callback onPress, Callback onRelease);
    void Stop();

    void HandleWindowMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    bool InstallHook();
    void RemoveHook();
    bool RequiredModifiersDown() const;

    static HotkeyManager* instance_;

    HHOOK hook_ = nullptr;
    HWND hwnd_ = nullptr;
    UINT id_ = 0;
    Hotkey hotkey_{};
    Callback onPress_;
    Callback onRelease_;
    bool hotkeyActive_ = false;
};

}  // namespace wflow
