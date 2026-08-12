param(
    [ValidateSet("ARM64", "x64")]
    [string]$Architecture = "ARM64",
    [string]$Configuration = "Release",
    [string]$BuildDir
)

$ErrorActionPreference = "Stop"

if (-not $BuildDir) {
    $BuildDir = if ($Architecture -eq "ARM64") { "build-arm64" } else { "build" }
}

$cmakeCandidates = @(
    "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\17\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

$cmake = $cmakeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cmake) {
    throw "CMake executable not found in known Visual Studio locations."
}

Write-Host "Using CMake: $cmake"
Write-Host "Architecture: $Architecture"
Write-Host "BuildDir: $BuildDir"

if (-not (Test-Path $BuildDir)) {
    $configureArgs = @(
        "-S", ".",
        "-B", $BuildDir,
        "-G", "Visual Studio 18 2026",
        "-A", $Architecture
    )
    & $cmake @configureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }
}

$buildArgs = @("--build", $BuildDir, "--config", $Configuration)
& $cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE"
}

Write-Host "Build complete: $BuildDir\$Configuration\wflow.exe"
