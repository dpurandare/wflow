param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build-arm64",
    [string]$PackageName = "wflow_0.1.0.0_arm64.msix",
    [ValidateSet("arm64", "x64")]
    [string]$Architecture = "arm64",
    [string]$Publisher = "CN=YourPublisher",
    [string]$Version = "0.1.0.0"
)

$ErrorActionPreference = "Stop"

function Find-MakeAppx {
    param(
        [string]$Architecture
    )

    $overridePath = [Environment]::GetEnvironmentVariable("WFLOW_MAKEAPPX_PATH")
    if (-not [string]::IsNullOrWhiteSpace($overridePath) -and (Test-Path $overridePath)) {
        return $overridePath
    }

    $archFolder = $Architecture.ToLowerInvariant()

    $candidates = @(
        ("${env:ProgramFiles(x86)}\\Windows Kits\\10\\bin\\" + $archFolder + "\\makeappx.exe"),
        ("$env:ProgramFiles\\Windows Kits\\10\\bin\\" + $archFolder + "\\makeappx.exe"),
        ("${env:ProgramFiles(x86)}\\Windows Kits\\10\\bin\\10.0.26100.0\\" + $archFolder + "\\makeappx.exe"),
        ("$env:ProgramFiles\\Windows Kits\\10\\bin\\10.0.26100.0\\" + $archFolder + "\\makeappx.exe"),
        ("${env:ProgramFiles(x86)}\\Windows Kits\\10\\bin\\10.0.22621.0\\" + $archFolder + "\\makeappx.exe"),
        ("$env:ProgramFiles\\Windows Kits\\10\\bin\\10.0.22621.0\\" + $archFolder + "\\makeappx.exe")
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }

    foreach ($root in @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "$env:ProgramFiles\Windows Kits\10\bin")) {
        if (-not (Test-Path $root)) {
            continue
        }

        $fallback = Get-ChildItem $root -Recurse -Filter makeappx.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match ("\\" + [regex]::Escape($archFolder) + "\\makeappx.exe$") } |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($fallback) {
            return $fallback.FullName
        }
    }

    throw "makeappx.exe not found for architecture '$Architecture'. Install Windows 10/11 SDK or set WFLOW_MAKEAPPX_PATH."
}

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$msixRoot = Resolve-Path $PSScriptRoot
$stagingDir = Join-Path $msixRoot "staging"
$outputDir = Join-Path $msixRoot "out"
$assetsDir = Join-Path $msixRoot "Assets"
$manifestPath = Join-Path $msixRoot "AppxManifest.xml"
$exePath = Join-Path $projectRoot "$BuildDir\$Configuration\wflow.exe"

$prereqScript = Join-Path $msixRoot "check-packaging-prereqs.ps1"
if (Test-Path $prereqScript) {
    & $prereqScript -Mode pack -Configuration $Configuration -BuildDir $BuildDir -Architecture $Architecture
}

if (-not (Test-Path $exePath)) {
    throw "Executable not found at $exePath. Build the app first."
}

if (-not (Test-Path $manifestPath)) {
    throw "Manifest not found at $manifestPath"
}

if (-not (Test-Path $assetsDir)) {
    throw "Assets folder not found at $assetsDir"
}

Remove-Item $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stagingDir | Out-Null
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

Copy-Item $exePath (Join-Path $stagingDir "wflow.exe")
Copy-Item $manifestPath (Join-Path $stagingDir "AppxManifest.xml")
Copy-Item $assetsDir (Join-Path $stagingDir "Assets") -Recurse -Force

$manifestFile = Join-Path $stagingDir "AppxManifest.xml"
[xml]$manifestXml = Get-Content $manifestFile -Raw

$identityNode = $manifestXml.Package.Identity
if (-not $identityNode) {
    throw "Identity node not found in AppxManifest.xml"
}

$identityNode.Publisher = $Publisher
$identityNode.Version = $Version

$manifestXml.Save($manifestFile)

$makeappx = Find-MakeAppx -Architecture $Architecture
$msixPath = Join-Path $outputDir $PackageName
if (Test-Path $msixPath) {
    Remove-Item $msixPath -Force
}

Write-Host "Using makeappx: $makeappx"
Write-Host "Staging from: $stagingDir"

Start-Process -FilePath $makeappx -ArgumentList @("pack", "/d", $stagingDir, "/p", $msixPath, "/o") -Wait -NoNewWindow

if (-not (Test-Path $msixPath)) {
    throw "MSIX package was not generated."
}

Write-Host "MSIX created: $msixPath"
Write-Host "Next: run sign-msix.ps1 to sign the package."
