# Extract OpenTUI C ABI / FFI signature hints for a given @opentui/core version.
# Prints zig.d.ts method lines and FFI args tables for key symbols.
param(
    [string]$Version = "0.5.10",
    [string[]]$Symbols = @(
        "createRenderer",
        "destroyRenderer",
        "setupTerminal",
        "setUseThread",
        "getNextBuffer",
        "getCurrentBuffer",
        "render",
        "resizeRenderer",
        "setBackgroundColor",
        "bufferClear",
        "bufferDrawText",
        "bufferDrawBox",
        "bufferFillRect",
        "getBufferWidth",
        "getBufferHeight",
        "writeOut",
        "setKittyKeyboardFlags",
        "disableKittyKeyboard"
    )
)

$ErrorActionPreference = "Stop"

$Work = Join-Path $env:TEMP ("opentui-ffi-hints-" + ($Version -replace '[^\w\.\-]', '_'))
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
New-Item -ItemType Directory -Force -Path $Work | Out-Null

Push-Location $Work
try {
    $spec = if ($Version -eq "latest") { "@opentui/core" } else { "@opentui/core@$Version" }
    Write-Host "=== Packing $spec ==="
    npm pack $spec --silent | Out-Null
    $Tgz = Get-ChildItem -Filter "opentui-core-*.tgz" | Select-Object -First 1
    if (-not $Tgz) { $Tgz = Get-ChildItem -Filter "*.tgz" | Select-Object -First 1 }
    if (-not $Tgz) { throw "npm pack failed for $spec" }

    tar -xf $Tgz.FullName
    $Pkg = Join-Path $Work "package"
    $pkgJson = Get-Content (Join-Path $Pkg "package.json") -Raw | ConvertFrom-Json
    Write-Host "Resolved version: $($pkgJson.version)"
    Write-Host ""

    $zigDts = Get-ChildItem -Path $Pkg -Recurse -Filter "zig.d.ts" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($zigDts) {
        Write-Host "=== zig.d.ts hits ($($zigDts.FullName)) ==="
        $text = Get-Content $zigDts.FullName -Raw
        foreach ($sym in $Symbols) {
            $pattern = [regex]::Escape($sym) + "\s*:\s*\([^;]+;"
            $m = [regex]::Match($text, $pattern)
            if ($m.Success) {
                Write-Host $m.Value.Trim()
            } else {
                Write-Host "$sym : (not found in zig.d.ts)"
            }
        }
        Write-Host ""
    } else {
        Write-Host "zig.d.ts not found in package"
        Write-Host ""
    }

    Write-Host "=== FFI args tables (bundled JS) ==="
    $jsFiles = Get-ChildItem -Path $Pkg -Recurse -Include "*.js","*.mjs" -ErrorAction SilentlyContinue
    foreach ($sym in $Symbols) {
        $found = $false
        foreach ($js in $jsFiles) {
            # Match: createRenderer: { args: [...], returns: "..." }
            $rx = [regex]::new(
                [regex]::Escape($sym) + '\s*:\s*\{\s*args\s*:\s*(\[[^\]]*\]|\[[\s\S]*?\])\s*,\s*returns\s*:\s*("[^"]+"|''[^'']+'')',
                [System.Text.RegularExpressions.RegexOptions]::Multiline
            )
            $content = Get-Content $js.FullName -Raw -ErrorAction SilentlyContinue
            if (-not $content) { continue }
            $m = $rx.Match($content)
            if ($m.Success) {
                $args = ($m.Groups[1].Value -replace '\s+', ' ').Trim()
                $ret = $m.Groups[2].Value
                Write-Host "$sym args=$args returns=$ret  ($($js.Name))"
                $found = $true
                break
            }
        }
        if (-not $found) {
            Write-Host "$sym : (FFI table not found)"
        }
    }

    Write-Host ""
    Write-Host "=== Hint ==="
    Write-Host "Map FFI types: u32/i32/u8/bool/ptr/buffer → C uint32_t/int32_t/uint8_t/bool/void*/uint16_t* as appropriate."
    Write-Host "Handle returns are usually u32 (not ptr) on current cores."
}
finally {
    Pop-Location
    Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue
}
