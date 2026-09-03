# Download @opentui/core-win32-x64 and extract opentui.dll into third_party/opentui/
param(
    [string]$Version = "0.5.10",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $OutDir) {
    $OutDir = Join-Path $Root "third_party\opentui"
}

$DllPath = Join-Path $OutDir "opentui.dll"
if ((Test-Path $DllPath) -and (Get-Item $DllPath).Length -gt 1MB) {
    Write-Host "opentui.dll already present: $DllPath"
    exit 0
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$Work = Join-Path $env:TEMP "opentui-fetch-$Version"
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
New-Item -ItemType Directory -Force -Path $Work | Out-Null

Push-Location $Work
try {
    Write-Host "Downloading @opentui/core-win32-x64@$Version ..."
    npm pack "@opentui/core-win32-x64@$Version" --silent | Out-Null
    $Tgz = Get-ChildItem -Filter "opentui-core-win32-x64-*.tgz" | Select-Object -First 1
    if (-not $Tgz) { throw "npm pack did not produce a tarball" }

    tar -xf $Tgz.FullName
    $SrcDll = Join-Path $Work "package\opentui.dll"
    if (-not (Test-Path $SrcDll)) { throw "opentui.dll missing from package" }

    Copy-Item -Force $SrcDll $DllPath
    Write-Host "Installed: $DllPath ($([math]::Round((Get-Item $DllPath).Length / 1MB, 1)) MB)"
}
finally {
    Pop-Location
    Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue
}
