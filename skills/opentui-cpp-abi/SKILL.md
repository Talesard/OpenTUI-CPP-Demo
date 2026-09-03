---
name: opentui-cpp-abi
description: >-
  Bootstraps or upgrades a C++ wrapper over OpenTUI’s Zig native C ABI for a
  given npm package version. Use when integrating OpenTUI without TypeScript,
  generating opentui.h, fetching opentui.dll / libopentui, or bumping the
  native core version.
---

# OpenTUI C++ ABI

Portable instructions for any coding agent. Teach yourself the quirks in
[reference.md](reference.md); run the scripts under [scripts/](scripts/).

## When to use

- New C/C++ project that should call OpenTUI’s native core directly
- Bump `@opentui/core-*` / refresh `opentui.h` against a new ABI
- User mentions OpenTUI C ABI, `opentui.dll`, or “no TypeScript / Bun”

## Hard rules

1. **Never invent FFI signatures.** Always extract them for the target version
   (`scripts/extract-ffi-hints.ps1` or by reading `@opentui/core@VER`).
2. Handles are `uint32_t`, not pointers. `0` is invalid.
3. Colors are packed `uint16_t[4]` (8-bit channel in the low byte).
4. `bufferDrawBox` border charset has **11** code points (see reference.md).
5. After `setupTerminal`, disable Kitty / modifyOtherKeys and drain capability
   replies before reading app keys.
6. Do not commit `third_party/`, `build/`, or `*.obj`.

## Workflow A — Bootstrap

Copy this checklist and track it:

```text
Bootstrap:
- [ ] Version + platform chosen
- [ ] Native lib fetched
- [ ] Import lib generated (Windows)
- [ ] FFI hints extracted
- [ ] opentui.h written
- [ ] CMake + minimal demo builds
- [ ] Smoke run in a real console
```

### 1. Resolve version and platform

- Version: user pin (e.g. `0.5.10`) or `latest` from npm.
- Platform default: `win32-x64`.
- Map platform → npm package / artifact (see reference.md).

### 2. Fetch the native library

From the **consumer project root** (or pass `-OutDir`):

```powershell
powershell -ExecutionPolicy Bypass -File path/to/skills/opentui-cpp-abi/scripts/fetch-native.ps1 `
  -Version 0.5.10 -Platform win32-x64 -OutDir third_party/opentui
```

### 3. Windows import library

With MSVC tools on `PATH` (`dumpbin`, `lib`):

```powershell
# Same approach as this repo's scripts/gen_implib.ps1
dumpbin /EXPORTS third_party/opentui/opentui.dll
# Write .def of export names, then:
lib /def:opentui.def /out:opentui.lib /machine:x64
```

Place `opentui.dll` next to the built `.exe` (CMake `POST_BUILD` copy).

### 4. Extract FFI hints (mandatory)

```powershell
powershell -ExecutionPolicy Bypass -File path/to/skills/opentui-cpp-abi/scripts/extract-ffi-hints.ps1 `
  -Version 0.5.10
```

Use the printed `zig.d.ts` / FFI `args` for at least:

`createRenderer`, `destroyRenderer`, `setupTerminal`, `getNextBuffer`, `render`,
`resizeRenderer`, `bufferClear`, `bufferDrawText`, `bufferDrawBox`, `writeOut`,
`setKittyKeyboardFlags`, `disableKittyKeyboard`.

Upstream source of truth when hacking Zig itself:
`https://github.com/anomalyco/opentui/blob/main/packages/native/src/lib.zig`

### 5. Emit `opentui.h`

Minimal C header:

- `OtuiHandle` = `uint32_t`
- `OtuiRGBA` = `uint16_t[4]` + `otui_rgb` helper
- Text attribute bitflags, destination / remote enums, box option bits
- `dllimport` on Windows for linked symbols
- Only declare symbols the app actually calls; extend later from FFI hints

### 6. App bootstrap pattern

```text
prepareConsole (VT processing on output; classic input)
createRenderer(w, h, OTUI_DEST_STDOUT, OTUI_REMOTE_LOCAL, NULL)
setUseThread(renderer, false)
setupTerminal(renderer, true)
claimInput:
  setKittyKeyboardFlags(0)
  disableKittyKeyboard
  writeOut("\x1b[>4;0m\x1b[<u")
  drain/flush console input
  disable ENABLE_VIRTUAL_TERMINAL_INPUT
loop:
  sync size via CONOUT$ srWindow → resizeRenderer if changed
  getNextBuffer → draw → render(force)
destroyRenderer(renderer, true)
```

CMake: C++17, link import lib (MSVC) or the shared lib (MinGW), copy DLL beside the exe.

### 7. Smoke test

Run in a **real console** (not a redirected pipe). Confirm alternate screen, text, quit key, and resize.

## Workflow B — Upgrade existing wrapper

```text
Upgrade:
- [ ] Target version chosen
- [ ] Native lib force-refetched
- [ ] FFI hints for new version
- [ ] Diff vs current opentui.h / call sites
- [ ] Header + call sites patched
- [ ] Rebuild + smoke
```

1. `fetch-native.ps1 -Version NEW -Force`
2. `extract-ffi-hints.ps1 -Version NEW` and compare to current declarations
3. Patch breaking changes (arity, types, bool vs u8, handle vs pointer eras)
4. Rebuild; fix compile errors; smoke-run

## Extending the header

When you need a new symbol:

1. Confirm it exists in FFI hints / `lib.zig` for that version.
2. Add the C declaration matching FFI widths (`u32`, `bool`/`u8`, `ptr`, buffers).
3. Call it; do not wrap the entire 400+ export surface unless required.

## Additional resources

- ABI quirks and platform package map: [reference.md](reference.md)
- Worked example in this repository: `include/opentui.h`, `src/otui_app.cpp`, `src/demo.cpp`
