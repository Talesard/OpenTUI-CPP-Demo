# Generate opentui.def + opentui.lib from opentui.dll (MSVC dumpbin/lib).
param(
    [Parameter(Mandatory = $true)][string]$Dll,
    [Parameter(Mandatory = $true)][string]$OutDir
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Dll)) { throw "DLL not found: $Dll" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$DefOut = Join-Path $OutDir "opentui.def"
$LibOut = Join-Path $OutDir "opentui.lib"

if ((Test-Path $LibOut) -and (Test-Path $DefOut)) {
    Write-Host "Import library already present: $LibOut"
    exit 0
}

$dumpbin = Get-Command dumpbin -ErrorAction SilentlyContinue
$lib = Get-Command lib -ErrorAction SilentlyContinue
if (-not $dumpbin -or -not $lib) {
    throw "dumpbin/lib not on PATH. Run from a Visual Studio Developer shell."
}

$dump = & dumpbin /EXPORTS $Dll | Out-String
$names = [regex]::Matches($dump, '(?m)^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+([A-Za-z_][A-Za-z0-9_]*)\s*$') |
    ForEach-Object { $_.Groups[1].Value } |
    Where-Object { $_ -notin @('Ordinal', 'Name', 'Hint', 'RVA') } |
    Select-Object -Unique

if ($names.Count -lt 10) {
    throw "Too few exports parsed from dumpbin ($($names.Count))"
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("LIBRARY opentui")
[void]$sb.AppendLine("EXPORTS")
foreach ($n in $names) {
    [void]$sb.AppendLine("    $n")
}
Set-Content -Path $DefOut -Value $sb.ToString() -Encoding ASCII
Write-Host "Wrote $($names.Count) exports to $DefOut"

& lib "/def:$DefOut" "/out:$LibOut" /machine:x64
if ($LASTEXITCODE -ne 0) { throw "lib /def failed with exit $LASTEXITCODE" }
Write-Host "Created $LibOut"
