#include "app/App.h"

#include <shellapi.h>

namespace wflow {

constexpr wchar_t App::kWindowClassName[];

App::App(HINSTANCE instance) : instance_(instance) {}

int App::Run() {
    if (!InitializeWindow()) {
        return 1;
    }

    if (!InitializeComponents()) {
        return 1;
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    hotkeyManager_.Stop();

    if (transcriptionThread_.joinable()) {
        transcriptionThread_.join();
    }

    if (modelDownloadThread_.joinable()) {
        modelDownloadThread_.join();
    }

    trayIcon_.Destroy();
    return static_cast<int>(message.wParam);
}

bool App::InitializeWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::WndProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&wc) == 0) {
        return false;
    }

    hwnd_ = CreateWindowExW(0,
                            kWindowClassName,
                            L"wflow",
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            nullptr,
                            nullptr,
                            instance_,
                            this);

    return hwnd_ != nullptr;
}

bool App::InitializeComponents() {
    settings_ = settingsStore_.Load();
    settings_.startWithWindows = startupManager_.IsEnabled();

    EnsureModelDirectory();

    (void)settingsStore_.Save(settings_);

    if (settings_.whisperLanguage.empty()) {
        settings_.whisperLanguage = "en";
    }

    if (settings_.whisperThreads <= 0) {
        settings_.whisperThreads = 4;
    }

#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    if (!EnsureWhisperModelReady()) {
        return false;
    }
#endif

    if (!trayIcon_.Create(hwnd_, kTrayMessage)) {
        return false;
    }

    trayIcon_.SetHotkeyText(HotkeyToString(settings_.holdToTalkHotkey));

    if (!hotkeyManager_.Start(
            hwnd_,
            kHotkeyId,
            settings_.holdToTalkHotkey,
            [this] { OnHotkeyPressed(); },
            [this] { OnHotkeyReleased(); })) {
        MessageBoxW(hwnd_,
                    L"Failed to register the global hotkey. Change it in settings once available.",
                    L"wflow",
                    MB_ICONWARNING | MB_OK);
    }

    TranscriberConfig transcriberConfig;
    transcriberConfig.modelPath = settings_.whisperModelPath;
    transcriberConfig.language = settings_.whisperLanguage;
    transcriberConfig.threads = settings_.whisperThreads;

    if (!transcriber_.Initialize(transcriberConfig)) {
        UpdateState(AppState::Error);
        std::wstring error = L"Failed to initialize local transcription engine.";
        if (!transcriber_.LastError().empty()) {
            error += L"\n\n" + transcriber_.LastError();
        }
        MessageBoxW(hwnd_, error.c_str(), L"wflow", MB_ICONERROR | MB_OK);
        return false;
    }

    UpdateState(AppState::Idle);
    return true;
}

std::wstring App::HotkeyToString(const Hotkey& hotkey) const {
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

    UINT scanCode = MapVirtualKeyW(hotkey.vk, MAPVK_VK_TO_VSC);
    wchar_t keyName[64] = {};
    if (GetKeyNameTextW(static_cast<LONG>(scanCode << 16), keyName, static_cast<int>(std::size(keyName))) > 0) {
        display += keyName;
    } else {
        display += L"Key " + std::to_wstring(hotkey.vk);
    }
    return display;
}

#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
bool App::EnsureWhisperModelReady() {
    if (std::filesystem::exists(settings_.whisperModelPath)) {
        return true;
    }

    const int choice = MessageBoxW(
        hwnd_,
        L"Whisper model file was not found.\n\nYes: Download default base.en model now\nNo: Open settings to pick a model path\nCancel: Exit",
        L"wflow",
        MB_ICONQUESTION | MB_YESNOCANCEL);

    if (choice == IDCANCEL) {
        return false;
    }

    if (choice == IDYES) {
        std::wstring error;
        if (!ModelBootstrap::DownloadDefaultModel(settings_.whisperModelPath, error, [this](int progress) {
                trayIcon_.SetStatusText(L"Downloading model " + std::to_wstring(progress) + L"%");
            })) {
            std::wstring message = L"Failed to download default Whisper model.";
            if (!error.empty()) {
                message += L"\n\n" + error;
            }
            MessageBoxW(hwnd_, message.c_str(), L"wflow", MB_ICONERROR | MB_OK);
            return false;
        }

        (void)settingsStore_.Save(settings_);
        return true;
    }

    std::optional<AppSettings> updated = settingsWindow_.ShowModal(instance_, hwnd_, settings_);
    if (!updated.has_value()) {
        return false;
    }

    settings_ = *updated;
    EnsureModelDirectory();
    (void)settingsStore_.Save(settings_);
    return std::filesystem::exists(settings_.whisperModelPath);
}
#endif

void App::OnHotkeyPressed() {
    if (modelDownloadInProgress_) {
        return;
    }

    if (state_ == AppState::Idle) {
        StartListening();
    }
}

void App::OnHotkeyReleased() {
    if (state_ == AppState::Listening) {
        StopListeningAndTranscribe();
    }
}

void App::StartListening() {
    if (modelDownloadInProgress_) {
        MessageBoxW(hwnd_,
                    L"Model download is in progress. Please wait for it to finish before dictating.",
                    L"wflow",
                    MB_ICONINFORMATION | MB_OK);
        return;
    }

    if (!audioCapture_.Start()) {
        UpdateState(AppState::Error);
        MessageBoxW(hwnd_, L"Failed to start microphone capture.", L"wflow", MB_ICONERROR | MB_OK);
        return;
    }

    UpdateState(AppState::Listening);
}

void App::StopListeningAndTranscribe() {
    const std::vector<float> samples = audioCapture_.Stop();

    constexpr size_t kMinimumUtteranceSamples = 16000 / 4;
    if (samples.size() < kMinimumUtteranceSamples) {
        UpdateState(AppState::Idle);
        return;
    }

    UpdateState(AppState::Transcribing);

    if (transcriptionThread_.joinable()) {
        transcriptionThread_.join();
    }

    transcriptionThread_ = std::thread([this, samples]() {
        const std::wstring transcript = transcriber_.Transcribe(samples);
        {
            std::lock_guard<std::mutex> lock(transcriptionMutex_);
            pendingTranscript_ = transcript;
        }
        PostMessageW(hwnd_, kTranscriptReadyMessage, 0, 0);
    });
}

void App::OnTranscriptReady() {
    std::wstring transcript;
    {
        std::lock_guard<std::mutex> lock(transcriptionMutex_);
        transcript = pendingTranscript_;
        pendingTranscript_.clear();
    }

    if (!transcript.empty()) {
        if (!textInjector_.InsertText(transcript, settings_.enableClipboardFallback)) {
            MessageBoxW(hwnd_,
                        L"Transcription completed, but wflow could not insert text into the target app. Try enabling clipboard fallback in Settings and retry.",
                        L"wflow",
                        MB_ICONWARNING | MB_OK);
        }
    }

    if (transcriptionThread_.joinable()) {
        transcriptionThread_.join();
    }

    UpdateState(AppState::Idle);
}

void App::UpdateState(AppState newState) {
    state_ = newState;
    UpdateTrayStatus();
}

void App::UpdateTrayStatus() {
    switch (state_) {
    case AppState::InitializingModel:
        trayIcon_.SetStatusText(L"Initializing");
        break;
    case AppState::Idle:
        trayIcon_.SetStatusText(L"Ready");
        break;
    case AppState::Listening:
        trayIcon_.SetStatusText(L"Listening");
        break;
    case AppState::Transcribing:
        trayIcon_.SetStatusText(L"Transcribing");
        break;
    case AppState::Error:
        trayIcon_.SetStatusText(L"Error");
        break;
    }
}

void App::EnsureModelDirectory() const {
    if (settings_.whisperModelPath.empty()) {
        return;
    }

    std::filesystem::path path(settings_.whisperModelPath);
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
}

bool App::ApplyHotkeyOrRollback(const AppSettings& previousSettings) {
    if (settings_.holdToTalkHotkey.modifiers == previousSettings.holdToTalkHotkey.modifiers &&
        settings_.holdToTalkHotkey.vk == previousSettings.holdToTalkHotkey.vk) {
        return true;
    }

    const bool applied = hotkeyManager_.Start(
        hwnd_,
        kHotkeyId,
        settings_.holdToTalkHotkey,
        [this] { OnHotkeyPressed(); },
        [this] { OnHotkeyReleased(); });

    if (applied) {
        trayIcon_.SetHotkeyText(HotkeyToString(settings_.holdToTalkHotkey));
        return true;
    }

    settings_.holdToTalkHotkey = previousSettings.holdToTalkHotkey;
    (void)hotkeyManager_.Start(
        hwnd_,
        kHotkeyId,
        settings_.holdToTalkHotkey,
        [this] { OnHotkeyPressed(); },
        [this] { OnHotkeyReleased(); });
    trayIcon_.SetHotkeyText(HotkeyToString(settings_.holdToTalkHotkey));

    MessageBoxW(hwnd_,
                L"Failed to register the selected hotkey. Previous hotkey was restored.",
                L"wflow settings",
                MB_ICONWARNING | MB_OK);
    return false;
}

void App::ReinitializeTranscriberOrShowError(const AppSettings& previousSettings) {
    TranscriberConfig config;
    config.modelPath = settings_.whisperModelPath;
    config.language = settings_.whisperLanguage;
    config.threads = settings_.whisperThreads;

    if (transcriber_.Initialize(config)) {
        return;
    }

    settings_ = previousSettings;
    settingsStore_.Save(settings_);

    TranscriberConfig rollback;
    rollback.modelPath = settings_.whisperModelPath;
    rollback.language = settings_.whisperLanguage;
    rollback.threads = settings_.whisperThreads;
    (void)transcriber_.Initialize(rollback);

    std::wstring error = L"Failed to apply transcription settings. Previous values were restored.";
    if (!transcriber_.LastError().empty()) {
        error += L"\n\n" + transcriber_.LastError();
    }
    MessageBoxW(hwnd_, error.c_str(), L"wflow settings", MB_ICONWARNING | MB_OK);
}

void App::DownloadDefaultModelFromTray() {
#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    if (state_ == AppState::Listening || state_ == AppState::Transcribing) {
        MessageBoxW(hwnd_,
                    L"Stop dictation before downloading a model.",
                    L"wflow",
                    MB_ICONINFORMATION | MB_OK);
        return;
    }

    if (modelDownloadInProgress_) {
        MessageBoxW(hwnd_, L"Model download is already in progress.", L"wflow", MB_ICONINFORMATION | MB_OK);
        return;
    }

    if (modelDownloadThread_.joinable()) {
        modelDownloadThread_.join();
    }

    EnsureModelDirectory();
    modelDownloadInProgress_ = true;
    modelDownloadSucceeded_ = false;
    {
        std::lock_guard<std::mutex> lock(modelDownloadMutex_);
        modelDownloadError_.clear();
    }

    trayIcon_.SetStatusText(L"Downloading model 0%");

    const std::wstring targetPath = settings_.whisperModelPath;
    modelDownloadThread_ = std::thread([this, targetPath]() {
        std::wstring error;
        const bool ok = ModelBootstrap::DownloadDefaultModel(targetPath, error, [this](int progress) {
            PostMessageW(hwnd_, kModelDownloadProgressMessage, static_cast<WPARAM>(progress), 0);
        });

        {
            std::lock_guard<std::mutex> lock(modelDownloadMutex_);
            modelDownloadSucceeded_ = ok;
            modelDownloadError_ = error;
        }

        PostMessageW(hwnd_, kModelDownloadCompletedMessage, 0, 0);
    });
#else
    MessageBoxW(hwnd_,
                L"This build does not include whisper.cpp. Reconfigure with WFLOW_USE_WHISPERCPP=ON to use local model download.",
                L"wflow",
                MB_ICONINFORMATION | MB_OK);
#endif
}

void App::OnModelDownloadProgress(int progressPercent) {
#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    if (progressPercent < 0) {
        trayIcon_.SetStatusText(L"Downloading model");
        return;
    }

    if (progressPercent > 100) {
        progressPercent = 100;
    }

    trayIcon_.SetStatusText(L"Downloading model " + std::to_wstring(progressPercent) + L"%");
#else
    (void)progressPercent;
#endif
}

void App::OnModelDownloadCompleted() {
#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    if (modelDownloadThread_.joinable()) {
        modelDownloadThread_.join();
    }

    bool ok = false;
    std::wstring error;
    {
        std::lock_guard<std::mutex> lock(modelDownloadMutex_);
        ok = modelDownloadSucceeded_;
        error = modelDownloadError_;
    }
    modelDownloadInProgress_ = false;

    if (!ok) {
        trayIcon_.SetStatusText(L"Ready");
        std::wstring message = L"Failed to download default Whisper model.";
        if (!error.empty()) {
            message += L"\n\n" + error;
        }
        MessageBoxW(hwnd_, message.c_str(), L"wflow", MB_ICONERROR | MB_OK);
        return;
    }

    if (!settingsStore_.Save(settings_)) {
        MessageBoxW(hwnd_,
                    L"Model downloaded, but saving settings failed.",
                    L"wflow",
                    MB_ICONWARNING | MB_OK);
    }

    trayIcon_.SetStatusText(L"Reinitializing model");

    const AppSettings previous = settings_;
    ReinitializeTranscriberOrShowError(previous);
    UpdateTrayStatus();
#endif
}

void App::ShowSettingsStub() {
    const AppSettings previous = settings_;
    std::optional<AppSettings> updated = settingsWindow_.ShowModal(instance_, hwnd_, settings_);
    if (!updated.has_value()) {
        return;
    }

    settings_ = *updated;
    EnsureModelDirectory();

    const bool hotkeyApplied = ApplyHotkeyOrRollback(previous);

    if (!startupManager_.SetEnabled(settings_.startWithWindows)) {
        MessageBoxW(hwnd_, L"Failed to update startup setting.", L"wflow settings", MB_ICONWARNING | MB_OK);
        settings_.startWithWindows = startupManager_.IsEnabled();
    }

    if (settings_.whisperModelPath != previous.whisperModelPath ||
        settings_.whisperLanguage != previous.whisperLanguage ||
        settings_.whisperThreads != previous.whisperThreads) {
        ReinitializeTranscriberOrShowError(previous);
    }

    if (hotkeyApplied) {
        if (!settingsStore_.Save(settings_)) {
            MessageBoxW(hwnd_, L"Failed to save settings file.", L"wflow settings", MB_ICONWARNING | MB_OK);
        }
    }
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = reinterpret_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    if (!app) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    if (message == kTrayMessage) {
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT point;
            GetCursorPos(&point);
            app->trayIcon_.ShowContextMenu(hwnd, point);
            return 0;
        }

        if (lParam == WM_LBUTTONUP) {
            if (app->state_ == AppState::Listening) {
                app->StopListeningAndTranscribe();
            } else if (app->state_ == AppState::Idle) {
                app->StartListening();
            }
            return 0;
        }
    }

    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case TrayIcon::kCommandToggleListening:
            if (app->state_ == AppState::Listening) {
                app->StopListeningAndTranscribe();
            } else if (app->state_ == AppState::Idle) {
                app->StartListening();
            }
            return 0;
        case TrayIcon::kCommandSettings:
            app->ShowSettingsStub();
            return 0;
        case TrayIcon::kCommandDownloadModel:
            app->DownloadDefaultModelFromTray();
            return 0;
        case TrayIcon::kCommandQuit:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_HOTKEY:
        app->hotkeyManager_.HandleWindowMessage(message, wParam, lParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case kTranscriptReadyMessage:
        app->OnTranscriptReady();
        return 0;
    case kModelDownloadProgressMessage:
        app->OnModelDownloadProgress(static_cast<int>(wParam));
        return 0;
    case kModelDownloadCompletedMessage:
        app->OnModelDownloadCompleted();
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace wflow
