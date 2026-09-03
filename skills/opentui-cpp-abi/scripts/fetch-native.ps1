# Download OpenTUI native shared library for a given version/platform.
param(
    [string]$Version = "0.5.10",
    [ValidateSet("win32-x64", "win32-arm64", "darwin-x64", "darwin-arm64", "linux-x64", "linux-arm64", "linux-x64-musl", "linux-arm64-musl")]
    [string]$Platform = "win32-x64",
    [string]$OutDir = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$PackageMap = @{
    "win32-x64"        = @{ Package = "@opentui/core-win32-x64";        Artifact = "opentui.dll" }
    "win32-arm64"      = @{ Package = "@opentui/core-win32-arm64";      Artifact = "opentui.dll" }
    "darwin-x64"       = @{ Package = "@opentui/core-darwin-x64";       Artifact = "libopentui.dylib" }
    "darwin-arm64"     = @{ Package = "@opentui/core-darwin-arm64";     Artifact = "libopentui.dylib" }
    "linux-x64"        = @{ Package = "@opentui/core-linux-x64";        Artifact = "libopentui.so" }
    "linux-arm64"      = @{ Package = "@opentui/core-linux-arm64";      Artifact = "libopentui.so" }
    "linux-x64-musl"   = @{ Package = "@opentui/core-linux-x64-musl";   Artifact = "libopentui.so" }
    "linux-arm64-musl" = @{ Package = "@opentui/core-linux-arm64-musl"; Artifact = "libopentui.so" }
}

$entry = $PackageMap[$Platform]
$Package = $entry.Package
$Artifact = $entry.Artifact

if (-not $OutDir) {
    $OutDir = Join-Path (Get-Location) "third_party/opentui"
}

$DestPath = Join-Path $OutDir $Artifact
if (-not $Force -and (Test-Path $DestPath) -and (Get-Item $DestPath).Length -gt 1MB) {
    Write-Host "Already present: $DestPath"
    exit 0
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$Work = Join-Path $env:TEMP ("opentui-fetch-" + $Platform + "-" + ($Version -replace '[^\w\.\-]', '_'))
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
New-Item -ItemType Directory -Force -Path $Work | Out-Null

Push-Location $Work
try {
    $spec = if ($Version -eq "latest") { $Package } else { "$Package@$Version" }
    Write-Host "Downloading $spec ..."
    npm pack $spec --silent | Out-Null
    $Tgz = Get-ChildItem -Filter "*.tgz" | Select-Object -First 1
    if (-not $Tgz) { throw "npm pack did not produce a tarball for $spec" }

    tar -xf $Tgz.FullName
    $Src = Join-Path $Work "package\$Artifact"
    if (-not (Test-Path $Src)) {
        $found = Get-ChildItem -Path (Join-Path $Work "package") -Recurse -Filter $Artifact -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($found) { $Src = $found.FullName }
    }
    if (-not (Test-Path $Src)) { throw "$Artifact missing from $spec" }

    Copy-Item -Force $Src $DestPath
    Write-Host "Installed: $DestPath ($([math]::Round((Get-Item $DestPath).Length / 1MB, 1)) MB)"
    Write-Host "Package=$Package Version=$Version Platform=$Platform"
}
finally {
    Pop-Location
    Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue
}
