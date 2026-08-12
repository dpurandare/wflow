# MSIX Packaging Scaffold

This folder contains a starter MSIX manifest for wflow.

## What is included

- App identity and metadata placeholder
- Full-trust desktop entry
- Microphone capability declaration

## What you must customize

1. Publisher
- Replace `CN=YourPublisher` with your signing certificate subject.

2. Assets
- Add logos in `packaging/msix/Assets`:
  - `StoreLogo.png`
  - `Square150x150Logo.png`
  - `Square44x44Logo.png`

3. Executable path
- If your packaged binary path differs, update `Executable` in `AppxManifest.xml`.

## Build guidance

Use Visual Studio/MSIX Packaging Tool or a CI packaging step to:
- stage binaries
- include `AppxManifest.xml`
- sign the package
- generate `.msix`

## Scripted workflow

Prerequisite:
- Install Windows 10/11 SDK so `makeappx.exe` and `signtool.exe` are available.

Optional preflight checks:

```powershell
./packaging/msix/check-packaging-prereqs.ps1 -Mode pack -Architecture arm64 -BuildDir build-arm64
./packaging/msix/check-packaging-prereqs.ps1 -Mode sign -Architecture arm64
./packaging/msix/check-packaging-prereqs.ps1 -Mode pack -Architecture x64 -BuildDir build-x64
./packaging/msix/check-packaging-prereqs.ps1 -Mode sign -Architecture x64
```

From the repository root:

1. Build app executable

ARM64:

```powershell
./scripts/build.ps1 -Architecture ARM64 -BuildDir build-arm64
```

x64:

```powershell
./scripts/build.ps1 -Architecture x64 -BuildDir build-x64
```

2. Create unsigned `.msix`

ARM64:

```powershell
./packaging/msix/build-msix.ps1 -Architecture arm64 -BuildDir build-arm64 -PackageName "wflow_0.1.0.0_arm64.msix" -Publisher "CN=YourPublisher" -Version "0.1.0.0"
```

x64:

```powershell
./packaging/msix/build-msix.ps1 -Architecture x64 -BuildDir build-x64 -PackageName "wflow_0.1.0.0_x64.msix" -Publisher "CN=YourPublisher" -Version "0.1.0.0"
```

Output:
- `packaging/msix/out/wflow_0.1.0.0_arm64.msix`
- `packaging/msix/out/wflow_0.1.0.0_x64.msix`

3. Sign package

ARM64:

```powershell
./packaging/msix/sign-msix.ps1 -Architecture arm64 -MsixPath ./packaging/msix/out/wflow_0.1.0.0_arm64.msix -PfxPath C:/path/to/cert.pfx
```

x64:

```powershell
./packaging/msix/sign-msix.ps1 -Architecture x64 -MsixPath ./packaging/msix/out/wflow_0.1.0.0_x64.msix -PfxPath C:/path/to/cert.pfx
```

The sign script prompts for the PFX password securely.

## Note on startup

Current startup implementation uses the HKCU Run key for unpackaged development builds.
For packaged production, use MSIX startup task extensions in the final manifest and app code path.

## CI release matrix

ARM64:

```powershell
./scripts/build.ps1 -Architecture ARM64 -BuildDir build-arm64 -Configuration Release
./packaging/msix/check-packaging-prereqs.ps1 -Mode pack -Architecture arm64 -BuildDir build-arm64
./packaging/msix/build-msix.ps1 -Architecture arm64 -BuildDir build-arm64 -Configuration Release -Publisher "CN=YourPublisher" -Version "0.1.0.0" -PackageName "wflow_0.1.0.0_arm64.msix"
./packaging/msix/sign-msix.ps1 -Architecture arm64 -MsixPath ./packaging/msix/out/wflow_0.1.0.0_arm64.msix -PfxPath C:/path/to/cert.pfx
```

x64:

```powershell
./scripts/build.ps1 -Architecture x64 -BuildDir build-x64 -Configuration Release
./packaging/msix/check-packaging-prereqs.ps1 -Mode pack -Architecture x64 -BuildDir build-x64
./packaging/msix/build-msix.ps1 -Architecture x64 -BuildDir build-x64 -Configuration Release -Publisher "CN=YourPublisher" -Version "0.1.0.0" -PackageName "wflow_0.1.0.0_x64.msix"
./packaging/msix/sign-msix.ps1 -Architecture x64 -MsixPath ./packaging/msix/out/wflow_0.1.0.0_x64.msix -PfxPath C:/path/to/cert.pfx
```
