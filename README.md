# wflow

wflow is an offline voice-to-text tray app for Windows. Hold a global hotkey, speak, release, and wflow inserts transcribed text into the focused app.

Initial development and validation is being done on ARM64 Windows. The project can also be built and packaged for x64 Windows.

## Architecture support

- ARM64: primary development target (actively validated)
- x64: supported build and packaging path
- x86 (32-bit): not currently validated in this repository

## Current feature status

- Win32 hidden-window app loop
- System tray icon with status menu
- Global hotkey handling with press and release lifecycle
- WASAPI microphone capture (mono normalization and 16 kHz resampling)
- Asynchronous transcription worker pipeline
- Unicode text insertion with SendInput
- Optional clipboard fallback insertion mode
- Persistent settings in %APPDATA%/wflow/settings.ini
- Startup toggle for unpackaged runs (HKCU Run key)
- Settings window for hotkey, model path, language, threads, startup, and fallback mode
- Optional whisper.cpp integration via CMake option WFLOW_USE_WHISPERCPP
- MSIX packaging scripts for ARM64 and x64

## Prerequisites

- Windows 11 (or Windows 10 with compatible SDK)
- Visual Studio with C++ toolchain
- CMake (Visual Studio bundled CMake is supported)
- Windows 10/11 SDK with:
	- makeappx.exe
	- signtool.exe

## Build from source

Run commands from repository root.

### ARM64 build

```powershell
./scripts/build.ps1 -Architecture ARM64 -BuildDir build-arm64 -Configuration Release
```

Run:

```powershell
.\build-arm64\Release\wflow.exe
```

### x64 build

```powershell
./scripts/build.ps1 -Architecture x64 -BuildDir build-x64 -Configuration Release
```

Run:

```powershell
.\build-x64\Release\wflow.exe
```

## Using the app

1. Launch wflow.
2. Open tray menu and go to Settings.
3. Set or confirm Whisper model path.
4. Optionally use Download Default Base Model.
5. Configure hotkey if needed.
6. In any text field: hold hotkey, speak, release to insert text.

## Create MSIX installer

The same flow works for ARM64 and x64 by changing the architecture and build directory.

### 1) Preflight checks

ARM64:

```powershell
./packaging/msix/check-packaging-prereqs.ps1 -Mode pack -Architecture arm64 -BuildDir build-arm64
./packaging/msix/check-packaging-prereqs.ps1 -Mode sign -Architecture arm64
```

x64:

```powershell
./packaging/msix/check-packaging-prereqs.ps1 -Mode pack -Architecture x64 -BuildDir build-x64
./packaging/msix/check-packaging-prereqs.ps1 -Mode sign -Architecture x64
```

### 2) Build executable

ARM64:

```powershell
./scripts/build.ps1 -Architecture ARM64 -BuildDir build-arm64 -Configuration Release
```

x64:

```powershell
./scripts/build.ps1 -Architecture x64 -BuildDir build-x64 -Configuration Release
```

### 3) Create unsigned MSIX

ARM64:

```powershell
./packaging/msix/build-msix.ps1 -Architecture arm64 -BuildDir build-arm64 -Configuration Release -Publisher "CN=YourPublisher" -Version "0.1.0.0" -PackageName "wflow_0.1.0.0_arm64.msix"
```

x64:

```powershell
./packaging/msix/build-msix.ps1 -Architecture x64 -BuildDir build-x64 -Configuration Release -Publisher "CN=YourPublisher" -Version "0.1.0.0" -PackageName "wflow_0.1.0.0_x64.msix"
```

Output files are created under packaging/msix/out.

### 4) Sign MSIX

ARM64:

```powershell
./packaging/msix/sign-msix.ps1 -Architecture arm64 -MsixPath ./packaging/msix/out/wflow_0.1.0.0_arm64.msix -PfxPath C:/path/to/cert.pfx
```

x64:

```powershell
./packaging/msix/sign-msix.ps1 -Architecture x64 -MsixPath ./packaging/msix/out/wflow_0.1.0.0_x64.msix -PfxPath C:/path/to/cert.pfx
```

The sign script prompts securely for the PFX password.

### 5) Install

Double-click the signed .msix file, or install with Add-AppxPackage in PowerShell.

## SDK path override (optional)

If auto-discovery cannot find SDK tools, set explicit paths:

```powershell
$env:WFLOW_MAKEAPPX_PATH = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\arm64\makeappx.exe"
$env:WFLOW_SIGNTOOL_PATH = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\arm64\signtool.exe"
```

For x64 packaging, point these to the x64 tool binaries.

## Notes

- Startup currently uses HKCU Run key for unpackaged development runs.
- For production packaged behavior, use MSIX startup task extensions.

## CI command matrix

Use these command sets in CI release jobs.

ARM64 release job:

```powershell
./scripts/build.ps1 -Architecture ARM64 -BuildDir build-arm64 -Configuration Release
./packaging/msix/check-packaging-prereqs.ps1 -Mode pack -Architecture arm64 -BuildDir build-arm64
./packaging/msix/build-msix.ps1 -Architecture arm64 -BuildDir build-arm64 -Configuration Release -Publisher "CN=YourPublisher" -Version "0.1.0.0" -PackageName "wflow_0.1.0.0_arm64.msix"
./packaging/msix/sign-msix.ps1 -Architecture arm64 -MsixPath ./packaging/msix/out/wflow_0.1.0.0_arm64.msix -PfxPath C:/path/to/cert.pfx
```

x64 release job:

```powershell
./scripts/build.ps1 -Architecture x64 -BuildDir build-x64 -Configuration Release
./packaging/msix/check-packaging-prereqs.ps1 -Mode pack -Architecture x64 -BuildDir build-x64
./packaging/msix/build-msix.ps1 -Architecture x64 -BuildDir build-x64 -Configuration Release -Publisher "CN=YourPublisher" -Version "0.1.0.0" -PackageName "wflow_0.1.0.0_x64.msix"
./packaging/msix/sign-msix.ps1 -Architecture x64 -MsixPath ./packaging/msix/out/wflow_0.1.0.0_x64.msix -PfxPath C:/path/to/cert.pfx
```
