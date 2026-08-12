# WFlow Implementation Plan

This file stores the execution plan in the repository root.

## Goal

Build a Windows-native tray application in C++ Win32 that mirrors mflow/lflow behavior:
- Hold global hotkey to record microphone audio
- Release hotkey to transcribe offline (whisper.cpp)
- Insert Unicode text into focused app via SendInput
- Ship as MSIX with startup support and advanced settings

## Phases

1. Architecture and scope lock
2. Core Win32 runtime skeleton
3. Global hotkey subsystem
4. WASAPI audio capture
5. Offline whisper.cpp transcription
6. Unicode text insertion
7. Settings UX and persistence
8. Startup integration
9. MSIX packaging and signing
10. Reliability/privacy hardening
11. Verification and release readiness

## Current progress

- Phase 2 started
- Initial app shell, tray menu, hotkey plumbing, and config scaffolding implemented
- Phase 4 in progress: WASAPI capture implemented with 16kHz output normalization
- Phase 6 in progress: Unicode SendInput injection path implemented
- Phase 8 started: startup toggle wiring for unpackaged runs implemented
- Phase 7 started: modal settings window implemented for transcription and startup options
- Phase 7 advanced: settings now support default model download action and hotkey test helper
- Phase 9 started: MSIX manifest scaffold added under packaging/msix
- Phase 9 advanced: scripted MSIX build/sign workflow added under packaging/msix
