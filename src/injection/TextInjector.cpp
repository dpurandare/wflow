#include "injection/TextInjector.h"

#include <windows.h>

#include <memory>
#include <vector>

namespace wflow {

namespace {

bool AnyModifierKeyDown() {
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
           (GetAsyncKeyState(VK_LCONTROL) & 0x8000) ||
           (GetAsyncKeyState(VK_RCONTROL) & 0x8000) ||
           (GetAsyncKeyState(VK_MENU) & 0x8000) ||
           (GetAsyncKeyState(VK_LMENU) & 0x8000) ||
           (GetAsyncKeyState(VK_RMENU) & 0x8000) ||
           (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
           (GetAsyncKeyState(VK_LSHIFT) & 0x8000) ||
           (GetAsyncKeyState(VK_RSHIFT) & 0x8000) ||
           (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
           (GetAsyncKeyState(VK_RWIN) & 0x8000);
}

void WaitForModifierKeysToRelease() {
    constexpr int kPollIntervalMs = 10;
    constexpr int kMaxWaitMs = 500;

    for (int waitedMs = 0; AnyModifierKeyDown() && waitedMs < kMaxWaitMs; waitedMs += kPollIntervalMs) {
        Sleep(kPollIntervalMs);
    }
}

}  // namespace

bool TextInjector::InsertText(const std::wstring& text, bool allowClipboardFallback) const {
    if (InsertUnicode(text)) {
        return true;
    }

    if (allowClipboardFallback) {
        return InsertViaClipboardPaste(text);
    }

    return false;
}

bool TextInjector::InsertUnicode(const std::wstring& text) const {
    if (text.empty()) {
        return true;
    }

    WaitForModifierKeysToRelease();

    std::vector<INPUT> inputs;
    inputs.reserve(text.size() * 2);

    for (wchar_t codeUnit : text) {
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = 0;
        down.ki.wScan = codeUnit;
        down.ki.dwFlags = KEYEVENTF_UNICODE;

        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

        inputs.push_back(down);
        inputs.push_back(up);
    }

    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    return sent == inputs.size();
}

bool TextInjector::InsertViaClipboardPaste(const std::wstring& text) const {
    if (text.empty()) {
        return true;
    }

    WaitForModifierKeysToRelease();

    if (!OpenClipboard(nullptr)) {
        return false;
    }

    std::wstring previousText;
    HANDLE previousData = GetClipboardData(CF_UNICODETEXT);
    if (previousData) {
        const wchar_t* previousRaw = static_cast<const wchar_t*>(GlobalLock(previousData));
        if (previousRaw) {
            previousText = previousRaw;
            GlobalUnlock(previousData);
        }
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }

    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        CloseClipboard();
        return false;
    }

    wchar_t* target = static_cast<wchar_t*>(GlobalLock(memory));
    if (!target) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    memcpy(target, text.c_str(), bytes);
    GlobalUnlock(memory);

    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();

    std::vector<INPUT> pasteInputs;
    pasteInputs.reserve(4);

    INPUT ctrlDown{};
    ctrlDown.type = INPUT_KEYBOARD;
    ctrlDown.ki.wVk = VK_CONTROL;

    INPUT vDown{};
    vDown.type = INPUT_KEYBOARD;
    vDown.ki.wVk = 'V';

    INPUT vUp = vDown;
    vUp.ki.dwFlags = KEYEVENTF_KEYUP;

    INPUT ctrlUp = ctrlDown;
    ctrlUp.ki.dwFlags = KEYEVENTF_KEYUP;

    pasteInputs.push_back(ctrlDown);
    pasteInputs.push_back(vDown);
    pasteInputs.push_back(vUp);
    pasteInputs.push_back(ctrlUp);

    const UINT sent = SendInput(static_cast<UINT>(pasteInputs.size()), pasteInputs.data(), sizeof(INPUT));
    const bool pasted = sent == pasteInputs.size();

    if (OpenClipboard(nullptr)) {
        if (previousText.empty()) {
            EmptyClipboard();
        } else {
            EmptyClipboard();
            const size_t restoreBytes = (previousText.size() + 1) * sizeof(wchar_t);
            HGLOBAL restoreMemory = GlobalAlloc(GMEM_MOVEABLE, restoreBytes);
            if (restoreMemory) {
                wchar_t* restoreTarget = static_cast<wchar_t*>(GlobalLock(restoreMemory));
                if (restoreTarget) {
                    memcpy(restoreTarget, previousText.c_str(), restoreBytes);
                    GlobalUnlock(restoreMemory);
                    if (!SetClipboardData(CF_UNICODETEXT, restoreMemory)) {
                        GlobalFree(restoreMemory);
                    }
                } else {
                    GlobalFree(restoreMemory);
                }
            }
        }
        CloseClipboard();
    }

    return pasted;
}

}  // namespace wflow
