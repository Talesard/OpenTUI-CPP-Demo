#pragma once

/*
 * Minimal C ABI bindings for OpenTUI's native Zig core (opentui.dll).
 * Signatures match packages/native/src/lib.zig (v0.5.x handle-based API).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#  ifdef OTUI_STATIC_DECLS
#    define OTUI_API
#  else
#    define OTUI_API __declspec(dllimport)
#  endif
#else
#  define OTUI_API
#endif

/** Opaque object handle (generation + kind packed into u32). 0 = invalid. */
typedef uint32_t OtuiHandle;

/** Packed RGBA: each u16 stores channel in low byte, metadata in high byte. */
typedef uint16_t OtuiRGBA[4];

enum {
  OTUI_ATTR_NONE = 0,
  OTUI_ATTR_BOLD = 1 << 0,
  OTUI_ATTR_DIM = 1 << 1,
  OTUI_ATTR_ITALIC = 1 << 2,
  OTUI_ATTR_UNDERLINE = 1 << 3,
  OTUI_ATTR_BLINK = 1 << 4,
  OTUI_ATTR_INVERSE = 1 << 5,
  OTUI_ATTR_HIDDEN = 1 << 6,
  OTUI_ATTR_STRIKETHROUGH = 1 << 7,
};

enum {
  OTUI_DEST_STDOUT = 0,
  OTUI_DEST_MEMORY = 1,
};

enum {
  OTUI_REMOTE_AUTO = 0,
  OTUI_REMOTE_LOCAL = 1,
  OTUI_REMOTE_REMOTE = 2,
};

/** All sides + fill + center title alignment. */
enum {
  OTUI_BOX_TOP = 1u << 3,
  OTUI_BOX_RIGHT = 1u << 2,
  OTUI_BOX_BOTTOM = 1u << 1,
  OTUI_BOX_LEFT = 1u << 0,
  OTUI_BOX_FILL = 1u << 4,
  OTUI_BOX_TITLE_CENTER = 1u << 5,
  OTUI_BOX_ALL_SIDES = OTUI_BOX_TOP | OTUI_BOX_RIGHT | OTUI_BOX_BOTTOM | OTUI_BOX_LEFT,
};

/** Build a literal RGB color (intent = rgb, metadata = 0). */
static inline void otui_rgb(OtuiRGBA out, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  out[0] = (uint16_t)r;
  out[1] = (uint16_t)g;
  out[2] = (uint16_t)b;
  out[3] = (uint16_t)a;
}

OTUI_API OtuiHandle createRenderer(
    uint32_t width,
    uint32_t height,
    uint8_t bufferedDestinationKind,
    uint8_t remoteModeValue,
    void *feedPtr);

OTUI_API void destroyRenderer(OtuiHandle renderer, bool flush_input);
OTUI_API void setUseThread(OtuiHandle renderer, bool useThread);
OTUI_API void setupTerminal(OtuiHandle renderer, bool useAlternateScreen);
OTUI_API void setBackgroundColor(OtuiHandle renderer, const uint16_t *color);
OTUI_API uint8_t render(OtuiHandle renderer, bool force);
OTUI_API OtuiHandle getNextBuffer(OtuiHandle renderer);
OTUI_API OtuiHandle getCurrentBuffer(OtuiHandle renderer);
OTUI_API void resizeRenderer(OtuiHandle renderer, uint32_t width, uint32_t height);
OTUI_API void setKittyKeyboardFlags(OtuiHandle renderer, uint8_t flags);
OTUI_API void disableKittyKeyboard(OtuiHandle renderer);
OTUI_API void writeOut(OtuiHandle renderer, const uint8_t *data, uint32_t dataLen);

OTUI_API void bufferClear(OtuiHandle buffer, const uint16_t *bg);
OTUI_API void bufferDrawText(
    OtuiHandle buffer,
    const uint8_t *text,
    uint32_t textLen,
    uint32_t x,
    uint32_t y,
    const uint16_t *fg,
    const uint16_t *bg,
    uint32_t attributes);
OTUI_API void bufferFillRect(
    OtuiHandle buffer,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    const uint16_t *bg);
OTUI_API void bufferDrawBox(
    OtuiHandle buffer,
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height,
    const uint32_t *borderChars,
    uint32_t packedOptions,
    const uint16_t *borderColor,
    const uint16_t *backgroundColor,
    const uint16_t *titleColor,
    const uint8_t *title,
    uint32_t titleLen,
    const uint8_t *bottomTitle,
    uint32_t bottomTitleLen);

OTUI_API uint32_t getBufferWidth(OtuiHandle buffer);
OTUI_API uint32_t getBufferHeight(OtuiHandle buffer);

#ifdef __cplusplus
}
#endif
