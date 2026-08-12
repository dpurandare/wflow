param(
    [Parameter(Mandatory = $true)]
    [string]$MsixPath,

    [Parameter(Mandatory = $true)]
    [string]$PfxPath,

    [ValidateSet("arm64", "x64")]
    [string]$Architecture = "arm64",

    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"

$prereqScript = Join-Path $PSScriptRoot "check-packaging-prereqs.ps1"
if (Test-Path $prereqScript) {
    & $prereqScript -Mode sign -Architecture $Architecture
}

function Find-SignTool {
    param(
        [string]$Architecture
    )

    $overridePath = [Environment]::GetEnvironmentVariable("WFLOW_SIGNTOOL_PATH")
    if (-not [string]::IsNullOrWhiteSpace($overridePath) -and (Test-Path $overridePath)) {
        return $overridePath
    }

    $archFolder = $Architecture.ToLowerInvariant()

    $candidates = @(
        ("${env:ProgramFiles(x86)}\\Windows Kits\\10\\bin\\" + $archFolder + "\\signtool.exe"),
        ("$env:ProgramFiles\\Windows Kits\\10\\bin\\" + $archFolder + "\\signtool.exe"),
        ("${env:ProgramFiles(x86)}\\Windows Kits\\10\\bin\\10.0.26100.0\\" + $archFolder + "\\signtool.exe"),
        ("$env:ProgramFiles\\Windows Kits\\10\\bin\\10.0.26100.0\\" + $archFolder + "\\signtool.exe"),
        ("${env:ProgramFiles(x86)}\\Windows Kits\\10\\bin\\10.0.22621.0\\" + $archFolder + "\\signtool.exe"),
        ("$env:ProgramFiles\\Windows Kits\\10\\bin\\10.0.22621.0\\" + $archFolder + "\\signtool.exe")
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }

    foreach ($root in @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "$env:ProgramFiles\Windows Kits\10\bin")) {
        if (-not (Test-Path $root)) {
            continue
        }

        $fallback = Get-ChildItem $root -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match ("\\" + [regex]::Escape($archFolder) + "\\signtool.exe$") } |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($fallback) {
            return $fallback.FullName
        }
    }

    throw "signtool.exe not found for architecture '$Architecture'. Install Windows 10/11 SDK or set WFLOW_SIGNTOOL_PATH."
}

if (-not (Test-Path $MsixPath)) {
    throw "MSIX not found: $MsixPath"
}
if (-not (Test-Path $PfxPath)) {
    throw "PFX not found: $PfxPath"
}

$secure = Read-Host "Enter PFX password" -AsSecureString
$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
$plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)

$signtool = Find-SignTool -Architecture $Architecture
Write-Host "Using signtool: $signtool"

Start-Process -FilePath $signtool -ArgumentList @(
    "sign",
    "/fd", "SHA256",
    "/f", $PfxPath,
    "/p", $plain,
    "/tr", $TimestampUrl,
    "/td", "SHA256",
    $MsixPath
) -Wait -NoNewWindow

Write-Host "Signed: $MsixPath"
