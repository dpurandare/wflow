#include "platform/SettingsWindow.h"

#include <commdlg.h>
#include <windowsx.h>

#include <filesystem>
#include <string>

#include "stt/ModelBootstrap.h"

namespace wflow {

constexpr wchar_t SettingsWindow::kWindowClassName[];

namespace {

HMENU MenuId(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

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

std::wstring KeyName(UINT vkCode) {
    UINT scanCode = MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC);
    if (vkCode == VK_LEFT || vkCode == VK_UP || vkCode == VK_RIGHT || vkCode == VK_DOWN ||
        vkCode == VK_PRIOR || vkCode == VK_NEXT || vkCode == VK_END || vkCode == VK_HOME ||
        vkCode == VK_INSERT || vkCode == VK_DELETE) {
        scanCode |= 0x0100;
    }

    wchar_t keyName[64] = {};
    if (GetKeyNameTextW(static_cast<LONG>(scanCode << 16), keyName, static_cast<int>(std::size(keyName))) > 0) {
        return keyName;
    }

    return L"Key " + std::to_wstring(vkCode);
}

}  // namespace

std::optional<AppSettings> SettingsWindow::ShowModal(HINSTANCE instance, HWND owner, const AppSettings& current) {
    instance_ = instance;
    owner_ = owner;
    working_ = current;
    saved_ = false;

    if (!CreateWindowClass(instance_)) {
        return std::nullopt;
    }

    hwnd_ = CreateWindowExW(WS_EX_DLGMODALFRAME,
                            kWindowClassName,
                            L"wflow settings",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            560,
                            300,
                            owner_,
                            nullptr,
                            instance_,
                            this);
    if (!hwnd_) {
        return std::nullopt;
    }

    EnableWindow(owner_, FALSE);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    MSG message;
    while (IsWindow(hwnd_) && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd_, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    EnableWindow(owner_, TRUE);
    SetForegroundWindow(owner_);

    if (saved_) {
        return working_;
    }
    return std::nullopt;
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* view = reinterpret_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_CREATE:
        self->hwnd_ = hwnd;
        if (!self->CreateControls()) {
            DestroyWindow(hwnd);
        } else {
            self->PopulateFromSettings();
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kIdBrowseButton:
            self->OnBrowseModel();
            return 0;
        case kIdRecordHotkeyButton:
            self->OnRecordHotkey();
            return 0;
        case kIdDownloadModelButton:
            self->OnDownloadDefaultModel();
            return 0;
        case kIdTestHotkeyButton:
            self->OnTestHotkey();
            return 0;
        case kIdSaveButton:
            if (self->OnSave()) {
                self->saved_ = true;
                DestroyWindow(hwnd);
            }
            return 0;
        case kIdCancelButton:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (self->recordingHotkey_) {
            self->OnCaptureHotkey(static_cast<UINT>(wParam));
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool SettingsWindow::CreateWindowClass(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &SettingsWindow::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&wc) != 0) {
        return true;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool SettingsWindow::CreateControls() {
    auto createLabel = [this](const wchar_t* text, int x, int y, int w, int h) {
        return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, hwnd_, nullptr, instance_, nullptr);
    };

    createLabel(L"Hold-to-talk hotkey", 20, 18, 200, 20);
    hotkeyLabel_ = CreateWindowExW(0,
                                   L"STATIC",
                                   L"",
                                   WS_CHILD | WS_VISIBLE,
                                   20,
                                   40,
                                   300,
                                   20,
                                   hwnd_,
                                   MenuId(kIdHotkeyLabel),
                                   instance_,
                                   nullptr);

    recordHotkeyButton_ = CreateWindowExW(0,
                                          L"BUTTON",
                                          L"Record New Hotkey",
                                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                          330,
                                          36,
                                          210,
                                          24,
                                          hwnd_,
                                          MenuId(kIdRecordHotkeyButton),
                                          instance_,
                                          nullptr);

    testHotkeyButton_ = CreateWindowExW(0,
                                        L"BUTTON",
                                        L"Test Hotkey",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        330,
                                        64,
                                        210,
                                        24,
                                        hwnd_,
                                        MenuId(kIdTestHotkeyButton),
                                        instance_,
                                        nullptr);

    createLabel(L"Whisper model (.bin)", 20, 76, 200, 20);
    modelEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE,
                                 L"EDIT",
                                 L"",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 20,
                                 98,
                                 430,
                                 24,
                                 hwnd_,
                                 MenuId(kIdModelEdit),
                                 instance_,
                                 nullptr);

    CreateWindowExW(0,
                    L"BUTTON",
                    L"Browse...",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    460,
                    98,
                    80,
                    24,
                    hwnd_,
                    MenuId(kIdBrowseButton),
                    instance_,
                    nullptr);

    downloadModelButton_ = CreateWindowExW(0,
                                           L"BUTTON",
                                           L"Download Default Base Model",
                                           WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                           330,
                                           126,
                                           210,
                                           24,
                                           hwnd_,
                                           MenuId(kIdDownloadModelButton),
                                           instance_,
                                           nullptr);

    createLabel(L"Language (Whisper code, e.g. en)", 20, 132, 250, 20);
    languageEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE,
                                    L"EDIT",
                                    L"",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    20,
                                    154,
                                    120,
                                    24,
                                    hwnd_,
                                    MenuId(kIdLanguageEdit),
                                    instance_,
                                    nullptr);

    createLabel(L"Inference threads", 160, 132, 150, 20);
    threadsEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE,
                                   L"EDIT",
                                   L"",
                                   WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                   160,
                                   154,
                                   80,
                                   24,
                                   hwnd_,
                                   MenuId(kIdThreadsEdit),
                                   instance_,
                                   nullptr);

    startupCheckbox_ = CreateWindowExW(0,
                                       L"BUTTON",
                                       L"Start with Windows",
                                       WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       20,
                                       188,
                                       220,
                                       24,
                                       hwnd_,
                                       MenuId(kIdStartupCheckbox),
                                       instance_,
                                       nullptr);

    clipboardCheckbox_ = CreateWindowExW(0,
                                         L"BUTTON",
                                         L"Enable clipboard fallback",
                                         WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                         20,
                                         214,
                                         240,
                                         24,
                                         hwnd_,
                                         MenuId(kIdClipboardCheckbox),
                                         instance_,
                                         nullptr);

    CreateWindowExW(0,
                    L"BUTTON",
                    L"Save",
                    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                    360,
                    248,
                    80,
                    28,
                    hwnd_,
                    MenuId(kIdSaveButton),
                    instance_,
                    nullptr);

    CreateWindowExW(0,
                    L"BUTTON",
                    L"Cancel",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    450,
                    248,
                    80,
                    28,
                    hwnd_,
                    MenuId(kIdCancelButton),
                    instance_,
                    nullptr);

        return modelEdit_ && languageEdit_ && threadsEdit_ && startupCheckbox_ && clipboardCheckbox_ && hotkeyLabel_ &&
            recordHotkeyButton_ && downloadModelButton_ && testHotkeyButton_;
}

void SettingsWindow::PopulateFromSettings() {
    UpdateHotkeyLabel();

    SetWindowTextW(modelEdit_, working_.whisperModelPath.c_str());

    std::wstring language(working_.whisperLanguage.begin(), working_.whisperLanguage.end());
    SetWindowTextW(languageEdit_, language.c_str());

    SetWindowTextW(threadsEdit_, std::to_wstring(working_.whisperThreads).c_str());

    Button_SetCheck(startupCheckbox_, working_.startWithWindows ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(clipboardCheckbox_, working_.enableClipboardFallback ? BST_CHECKED : BST_UNCHECKED);
}

void SettingsWindow::UpdateHotkeyLabel() {
    if (!hotkeyLabel_) {
        return;
    }

    const std::wstring label = L"Current hotkey: " + HotkeyToString(working_.holdToTalkHotkey);
    SetWindowTextW(hotkeyLabel_, label.c_str());
}

void SettingsWindow::OnBrowseModel() {
    wchar_t fileBuffer[MAX_PATH] = {};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Whisper Model (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        SetWindowTextW(modelEdit_, fileBuffer);
    }
}

bool SettingsWindow::OnSave() {
    wchar_t modelPath[1024] = {};
    wchar_t language[64] = {};
    wchar_t threadsText[64] = {};

    GetWindowTextW(modelEdit_, modelPath, static_cast<int>(std::size(modelPath)));
    GetWindowTextW(languageEdit_, language, static_cast<int>(std::size(language)));
    GetWindowTextW(threadsEdit_, threadsText, static_cast<int>(std::size(threadsText)));

    const int threads = _wtoi(threadsText);
    if (threads <= 0) {
        MessageBoxW(hwnd_, L"Threads must be a positive integer.", L"wflow settings", MB_ICONWARNING | MB_OK);
        return false;
    }

    if (working_.holdToTalkHotkey.modifiers == 0 || IsModifierKey(working_.holdToTalkHotkey.vk)) {
        MessageBoxW(hwnd_,
                    L"Hotkey must include at least one modifier and a non-modifier key.",
                    L"wflow settings",
                    MB_ICONWARNING | MB_OK);
        return false;
    }

    if (modelPath[0] != L'\0' && !std::filesystem::exists(modelPath)) {
        const int result = MessageBoxW(hwnd_,
                                       L"The selected model file does not exist yet. Save anyway?",
                                       L"wflow settings",
                                       MB_ICONQUESTION | MB_YESNO);
        if (result != IDYES) {
            return false;
        }
    }

    working_.whisperModelPath = modelPath;

    std::wstring languageWide = language;
    if (languageWide.empty()) {
        languageWide = L"en";
    }
    working_.whisperLanguage = WideToUtf8(languageWide);
    if (working_.whisperLanguage.empty()) {
        working_.whisperLanguage = "en";
    }

    working_.whisperThreads = threads;
    working_.startWithWindows = Button_GetCheck(startupCheckbox_) == BST_CHECKED;
    working_.enableClipboardFallback = Button_GetCheck(clipboardCheckbox_) == BST_CHECKED;

    return true;
}

void SettingsWindow::OnDownloadDefaultModel() {
    wchar_t modelPath[1024] = {};
    GetWindowTextW(modelEdit_, modelPath, static_cast<int>(std::size(modelPath)));

    if (modelPath[0] == L'\0') {
        MessageBoxW(hwnd_,
                    L"Set a model file path first (for example ...\\ggml-base.en.bin).",
                    L"wflow settings",
                    MB_ICONWARNING | MB_OK);
        return;
    }

    EnableWindow(downloadModelButton_, FALSE);
    SetWindowTextW(downloadModelButton_, L"Downloading...");

    std::wstring error;
    const bool ok = ModelBootstrap::DownloadDefaultModel(modelPath, error);

    SetWindowTextW(downloadModelButton_, L"Download Default Base Model");
    EnableWindow(downloadModelButton_, TRUE);

    if (!ok) {
        std::wstring message = L"Model download failed.";
        if (!error.empty()) {
            message += L"\n\n" + error;
        }
        MessageBoxW(hwnd_, message.c_str(), L"wflow settings", MB_ICONERROR | MB_OK);
        return;
    }

    MessageBoxW(hwnd_,
                L"Model downloaded successfully. Click Save to persist this path.",
                L"wflow settings",
                MB_ICONINFORMATION | MB_OK);
}

void SettingsWindow::OnTestHotkey() {
    std::wstring message =
        L"Current hotkey: " + HotkeyToString(working_.holdToTalkHotkey) +
        L"\n\nClose settings and hold this hotkey in a text editor to verify dictation flow.";
    MessageBoxW(hwnd_, message.c_str(), L"wflow settings", MB_ICONINFORMATION | MB_OK);
}

void SettingsWindow::OnRecordHotkey() {
    recordingHotkey_ = !recordingHotkey_;
    if (recordingHotkey_) {
        SetWindowTextW(recordHotkeyButton_, L"Press keys... (Esc to cancel)");
        SetFocus(hwnd_);
    } else {
        SetWindowTextW(recordHotkeyButton_, L"Record New Hotkey");
    }
}

void SettingsWindow::OnCaptureHotkey(UINT vkCode) {
    if (vkCode == VK_ESCAPE) {
        recordingHotkey_ = false;
        SetWindowTextW(recordHotkeyButton_, L"Record New Hotkey");
        return;
    }

    if (IsModifierKey(vkCode)) {
        return;
    }

    UINT modifiers = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        modifiers |= MOD_CONTROL;
    }
    if (GetKeyState(VK_MENU) & 0x8000) {
        modifiers |= MOD_ALT;
    }
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        modifiers |= MOD_SHIFT;
    }
    if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) {
        modifiers |= MOD_WIN;
    }

    if (modifiers == 0) {
        MessageBoxW(hwnd_,
                    L"Include at least one modifier (Ctrl, Alt, Shift, or Win).",
                    L"wflow settings",
                    MB_ICONWARNING | MB_OK);
        return;
    }

    working_.holdToTalkHotkey.modifiers = modifiers;
    working_.holdToTalkHotkey.vk = vkCode;
    UpdateHotkeyLabel();

    recordingHotkey_ = false;
    SetWindowTextW(recordHotkeyButton_, L"Record New Hotkey");
}

bool SettingsWindow::IsModifierKey(UINT vkCode) {
    return vkCode == VK_LCONTROL || vkCode == VK_RCONTROL || vkCode == VK_CONTROL || vkCode == VK_LMENU ||
           vkCode == VK_RMENU || vkCode == VK_MENU || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT ||
           vkCode == VK_SHIFT || vkCode == VK_LWIN || vkCode == VK_RWIN;
}

std::wstring SettingsWindow::HotkeyToString(const Hotkey& hotkey) {
    std::wstring display;
    if (hotkey.modifiers & MOD_CONTROL) {
        display += L"Ctrl + ";
    }
    if (hotkey.modifiers & MOD_ALT) {
        display += L"Alt + ";
    }
    if (hotkey.modifiers & MOD_SHIFT) {
        display += L"Shift + ";
    }
    if (hotkey.modifiers & MOD_WIN) {
        display += L"Win + ";
    }
    display += KeyName(hotkey.vk);
    return display;
}

}  // namespace wflow
