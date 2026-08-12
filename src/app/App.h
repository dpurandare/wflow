#pragma once

#include <windows.h>

#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio/AudioCapture.h"
#include "config/SettingsStore.h"
#include "injection/TextInjector.h"
#include <atomic>
#include "platform/HotkeyManager.h"
#include "platform/SettingsWindow.h"
#include "platform/StartupManager.h"
#include "platform/TrayIcon.h"
#include "stt/ModelBootstrap.h"
#include "stt/Transcriber.h"

namespace wflow {

enum class AppState {
    InitializingModel,
    Idle,
    Listening,
    Transcribing,
    Error,
};

class App {
public:
    explicit App(HINSTANCE instance);
    int Run();

private:
    static constexpr wchar_t kWindowClassName[] = L"WFlowHiddenWindow";
    static constexpr UINT kModelDownloadProgressMessage = WM_APP + 3;
    static constexpr UINT kModelDownloadCompletedMessage = WM_APP + 4;
    static constexpr UINT kTrayMessage = WM_APP + 1;
    static constexpr UINT kTranscriptReadyMessage = WM_APP + 2;
    static constexpr UINT kHotkeyId = 1;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool InitializeWindow();
    bool InitializeComponents();

    void OnHotkeyPressed();
    void OnHotkeyReleased();
    void StartListening();
    void StopListeningAndTranscribe();
    void OnTranscriptReady();

    void EnsureModelDirectory() const;
    std::wstring HotkeyToString(const Hotkey& hotkey) const;
#if defined(WFLOW_USE_WHISPERCPP) && WFLOW_USE_WHISPERCPP
    bool EnsureWhisperModelReady();
#endif
    bool ApplyHotkeyOrRollback(const AppSettings& previousSettings);
    void OnModelDownloadProgress(int progressPercent);
    void OnModelDownloadCompleted();
    void ReinitializeTranscriberOrShowError(const AppSettings& previousSettings);
    void DownloadDefaultModelFromTray();

    void UpdateState(AppState newState);
    void UpdateTrayStatus();
    void ShowSettingsStub();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;

    AppState state_ = AppState::InitializingModel;

    SettingsStore settingsStore_;
    AppSettings settings_;

    AudioCapture audioCapture_;
    Transcriber transcriber_;
    TextInjector textInjector_;
    StartupManager startupManager_;
    std::thread modelDownloadThread_;

    std::mutex modelDownloadMutex_;
    std::thread transcriptionThread_;
    std::wstring modelDownloadError_;
    std::atomic<bool> modelDownloadInProgress_ = false;
    bool modelDownloadSucceeded_ = false;
    std::mutex transcriptionMutex_;
    std::wstring pendingTranscript_;

    SettingsWindow settingsWindow_;

    TrayIcon trayIcon_;
    HotkeyManager hotkeyManager_;
};

}  // namespace wflow
