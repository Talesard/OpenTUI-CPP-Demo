#pragma once

#include "opentui.h"

#include <cstdint>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace otui_app {

inline constexpr uint32_t kRoundedBorder[11] = {
    0x256D,  // topLeft
    0x256E,  // topRight
    0x2570,  // bottomLeft
    0x256F,  // bottomRight
    0x2500,  // horizontal
    0x2502,  // vertical
    0x252C,  // topT
    0x2534,  // bottomT
    0x251C,  // leftT
    0x2524,  // rightT
    0x253C,  // cross
};

void prepareConsole();
void showError(const char *title, const char *message);
bool consoleSize(uint32_t *width, uint32_t *height);

inline void drawText(OtuiHandle buffer, uint32_t x, uint32_t y, const char *text, const OtuiRGBA fg,
                     const OtuiRGBA *bg, uint32_t attrs) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(text);
  uint32_t len = 0;
  while (bytes[len] != 0) {
    ++len;
  }
  bufferDrawText(buffer, bytes, len, x, y, fg, bg ? *bg : nullptr, attrs);
}

struct KeyEvent {
  bool present = false;
  bool keyDown = false;
  unsigned virtualKey = 0;
  wchar_t ch = 0;
  bool ctrl = false;
};

// Call after OpenTUI setupTerminal(): disable Kitty/modifyOtherKeys and take over input.
void claimInput(OtuiHandle renderer);

// Non-blocking normalized key event (arrows/letters/etc).
KeyEvent pollKey();

// Poll console window size and call resizeRenderer when it changes.
// Updates *width/*height (clamped to mins). Returns true if a resize was applied.
bool syncRendererSize(OtuiHandle renderer, uint32_t *width, uint32_t *height, uint32_t min_width,
                      uint32_t min_height);

void flushInput();
void drainInput(unsigned milliseconds);

}  // namespace otui_app