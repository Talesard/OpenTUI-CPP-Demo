# OpenTUI C ABI reference

Facts agents need when wrapping the Zig native core from C or C++. Verify
signatures per version with `scripts/extract-ffi-hints.ps1`.

## Platform packages

| Platform | npm package | Shared library |
|----------|-------------|----------------|
| Windows x64 | `@opentui/core-win32-x64` | `opentui.dll` |
| Windows arm64 | `@opentui/core-win32-arm64` | `opentui.dll` |
| macOS x64 | `@opentui/core-darwin-x64` | `libopentui.dylib` |
| macOS arm64 | `@opentui/core-darwin-arm64` | `libopentui.dylib` |
| Linux x64 glibc | `@opentui/core-linux-x64` | `libopentui.so` |
| Linux arm64 glibc | `@opentui/core-linux-arm64` | `libopentui.so` |
| Linux x64 musl | `@opentui/core-linux-x64-musl` | `libopentui.so` |
| Linux arm64 musl | `@opentui/core-linux-arm64-musl` | `libopentui.so` |

Also pack `@opentui/core@VERSION` for TypeScript/`zig.d.ts` / FFI metadata (no
native binary required for hints).

This skill’s worked path is **Windows x64 + MSVC**. Other platforms use the same
ABI; only fetch path and linker flags change.

## Handles

- Renderer, buffer, and most objects are **`uint32_t` handles**, not raw pointers.
- Invalid / null sentinel: **`0`**.
- Older docs or third-party bindings may still say “pointer”; trust the version’s
  FFI (`returns: "u32"`) over blog posts.

## Colors (`OtuiRGBA`)

- Wire format: `uint16_t[4]` (RGBA).
- Each component: **low byte = 0–255 channel**, high byte = metadata nibble of a
  packed intent word (rgb / indexed / default).
- For literal RGB, set high bytes to `0` (helper: store `r,g,b,a` in low bytes).
- Pass `const uint16_t *` into draw/clear APIs; optional background may be null
  where the ABI allows (`?[*]const u16`).

## Text attributes

Base flags in the low 8 bits of a `uint32_t` (OR-able):

| Bit | Name |
|-----|------|
| 0 | bold |
| 1 | dim |
| 2 | italic |
| 3 | underline |
| 4 | blink |
| 5 | inverse |
| 6 | hidden |
| 7 | strikethrough |

Higher bits may pack link ids; leave them zero unless using the link API.

## `createRenderer`

Typical modern arity (confirm per version):

```text
createRenderer(width: u32, height: u32,
               bufferedDestinationKind: u8,
               remoteModeValue: u8,
               feedPtr: ptr|null) -> u32 handle
```

- `bufferedDestinationKind`: `0` = process stdout, `1` = memory.
- `remoteModeValue`: `0` auto, `1` local, `2` remote.
- Width/height must be non-zero or the call returns `0`.

## `bufferDrawBox` border charset

**11** `uint32_t` code points, index order:

| Index | Role |
|-------|------|
| 0 | topLeft |
| 1 | topRight |
| 2 | bottomLeft |
| 3 | bottomRight |
| 4 | horizontal |
| 5 | vertical |
| 6 | topT |
| 7 | bottomT |
| 8 | leftT |
| 9 | rightT |
| 10 | cross |

Rounded example:

```text
U+256D U+256E U+2570 U+256F U+2500 U+2502
U+252C U+2534 U+251C U+2524 U+253C
```

Do **not** use a 9-element “TL T TR L C R BL B BR” layout; it scrambles edges.

### Packed box options (common layout)

Confirm in `lib.zig` for the version; typical bits:

- bits 0–3: left / bottom / right / top sides
- bit 4: fill
- bits 5–6: title alignment (e.g. center)

## Frame loop

```text
buffer = getNextBuffer(renderer)
bufferClear / bufferDrawBox / bufferDrawText / …
render(renderer, force=true)
```

`getBufferWidth` / `getBufferHeight` on that buffer are the ground truth for
what the core is drawing after `resizeRenderer`.

## Input after `setupTerminal`

`setupTerminal` enables alternate screen and may enable Kitty keyboard /
modifyOtherKeys. Those rewrite keys into CSI sequences and flood the console
with capability replies.

Recommended claim-input sequence:

1. `setKittyKeyboardFlags(renderer, 0)`
2. `disableKittyKeyboard(renderer)`
3. `writeOut` `"\x1b[>4;0m"` (modifyOtherKeys off) and `"\x1b[<u"` (pop Kitty)
4. Flush / drain console input for ~200ms
5. Console input mode: enable window input; **clear**
   `ENABLE_VIRTUAL_TERMINAL_INPUT`, echo, line, processed; clear quick-edit

Parse both classic `VK_*` key events and simple CSI (`\x1b[A` …) if the host
still emits them.

## Console size (Windows)

- Query **`CONOUT$`**, not only `STD_OUTPUT_HANDLE` (more reliable if stdout is
  odd).
- Visible size: `srWindow.Right - Left + 1`, `Bottom - Top + 1` (not scrollback
  `dwSize.Y`).
- On change: `resizeRenderer(renderer, w, h)`.
- Displayed “Size” in demos should match buffer width/height when possible.

## Linking

| Toolchain | Approach |
|-----------|----------|
| MSVC | Generate `.lib` from DLL exports; `#pragma`/`__declspec(dllimport)`; copy DLL beside exe |
| MinGW | Link the DLL directly; copy beside exe |
| Linux/macOS | Link `libopentui.so` / `.dylib`; set `rpath` or `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` |

## VCS ignores

```text
/build/
/third_party/
*.obj *.exe *.pdb *.lib *.exp *.def
```

## Upstream

- Native exports: `packages/native/src/lib.zig` in [anomalyco/opentui](https://github.com/anomalyco/opentui)
- TS bindings / FFI maps: `@opentui/core` package (`zig.d.ts`, bundled JS FFI tables)
