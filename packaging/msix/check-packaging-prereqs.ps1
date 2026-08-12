param(
    [ValidateSet("pack", "sign")]
    [string]$Mode = "pack",
    [ValidateSet("arm64", "x64")]
    [string]$Architecture = "arm64",
    [string]$Configuration = "Release",
    [string]$BuildDir = "build-arm64"
)

$ErrorActionPreference = "Stop"

function Find-Tool([string]$toolName, [string]$architecture) {
    $overrideVar = switch ($toolName.ToLowerInvariant()) {
        "makeappx.exe" { "WFLOW_MAKEAPPX_PATH" }
        "signtool.exe" { "WFLOW_SIGNTOOL_PATH" }
        default { $null }
    }

    if ($overrideVar) {
        $overridePath = [Environment]::GetEnvironmentVariable($overrideVar)
        if (-not [string]::IsNullOrWhiteSpace($overridePath) -and (Test-Path $overridePath)) {
            return $overridePath
        }
    }

    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    )

    $archFolder = $architecture.ToLowerInvariant()

    foreach ($root in $roots) {
        if (-not (Test-Path $root)) {
            continue
        }

        $found = Get-ChildItem $root -Recurse -Filter $toolName -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match ("\\" + [regex]::Escape($archFolder) + "\\" + [regex]::Escape($toolName) + "$") } |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($found) {
            return $found.FullName
        }
    }

    return $null
}

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$msixRoot = Resolve-Path $PSScriptRoot
$manifestPath = Join-Path $msixRoot "AppxManifest.xml"
$assetsDir = Join-Path $msixRoot "Assets"
$exePath = Join-Path $projectRoot "$BuildDir\$Configuration\wflow.exe"

$checks = @()

$makeappx = Find-Tool "makeappx.exe" $Architecture
$signtool = Find-Tool "signtool.exe" $Architecture

$makeappxDetail = if ($makeappx) { $makeappx } else { "NOT FOUND" }
$signtoolDetail = if ($signtool) { $signtool } else { "NOT FOUND" }

if ($Mode -eq "pack") {
    $checks += [pscustomobject]@{ Name = "Target architecture"; Ok = $true; Detail = $Architecture }
    $checks += [pscustomobject]@{ Name = "makeappx.exe"; Ok = [bool]$makeappx; Detail = $makeappxDetail }
    $checks += [pscustomobject]@{ Name = "App executable"; Ok = (Test-Path $exePath); Detail = $exePath }
    $checks += [pscustomobject]@{ Name = "AppxManifest.xml"; Ok = (Test-Path $manifestPath); Detail = $manifestPath }
    $checks += [pscustomobject]@{ Name = "Assets directory"; Ok = (Test-Path $assetsDir); Detail = $assetsDir }

    foreach ($file in @("StoreLogo.png", "Square150x150Logo.png", "Square44x44Logo.png")) {
        $full = Join-Path $assetsDir $file
        $checks += [pscustomobject]@{ Name = "Asset: $file"; Ok = (Test-Path $full); Detail = $full }
    }
} else {
    $checks += [pscustomobject]@{ Name = "signtool.exe"; Ok = [bool]$signtool; Detail = $signtoolDetail }
}

$checks | ForEach-Object {
    $status = if ($_.Ok) { "OK" } else { "MISSING" }
    Write-Host ("[{0}] {1} - {2}" -f $status, $_.Name, $_.Detail)
}

$failed = @($checks | Where-Object { -not $_.Ok })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Packaging prerequisites are incomplete."
    if ($Mode -eq "pack") {
        Write-Host "Install Windows 10/11 SDK (for makeappx), build wflow, and add missing assets."
    } else {
        Write-Host "Install Windows 10/11 SDK (for signtool) and ensure your signing inputs are present."
    }
    throw "Packaging prerequisites check failed."
}

Write-Host ""
Write-Host "All packaging prerequisites are satisfied."
