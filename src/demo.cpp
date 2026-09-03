#include "opentui.h"
#include "otui_app.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

int main() {
  otui_app::prepareConsole();

  uint32_t width = 80;
  uint32_t height = 24;
  otui_app::consoleSize(&width, &height);
  if (width < 1) {
    width = 80;
  }
  if (height < 1) {
    height = 24;
  }

  const OtuiHandle renderer =
      createRenderer(width, height, OTUI_DEST_STDOUT, OTUI_REMOTE_LOCAL, nullptr);
  if (renderer == 0) {
    otui_app::showError("opentui_demo", "createRenderer failed. Is opentui.dll next to the exe?");
    return 1;
  }

  setUseThread(renderer, false);
  setupTerminal(renderer, true);
  otui_app::claimInput(renderer);

  OtuiRGBA bg{};
  OtuiRGBA panelBg{};
  OtuiRGBA border{};
  OtuiRGBA titleFg{};
  OtuiRGBA bodyFg{};
  OtuiRGBA accent{};
  otui_rgb(bg, 12, 14, 20, 255);
  otui_rgb(panelBg, 18, 22, 32, 255);
  otui_rgb(border, 80, 200, 255, 255);
  otui_rgb(titleFg, 255, 200, 80, 255);
  otui_rgb(bodyFg, 220, 230, 240, 255);
  otui_rgb(accent, 120, 255, 180, 255);

  setBackgroundColor(renderer, bg);

  const auto started = std::chrono::steady_clock::now();
  uint64_t frames = 0;
  bool running = true;

  while (running) {
    // Keep renderer matched to the real console window (no artificial min size).
    otui_app::syncRendererSize(renderer, &width, &height, 1, 1);

    const otui_app::KeyEvent key = otui_app::pollKey();
    if (key.present) {
      if (key.virtualKey == VK_ESCAPE || key.ch == L'q' || key.ch == L'Q') {
        running = false;
        break;
      }
    }

    const OtuiHandle buffer = getNextBuffer(renderer);
    if (buffer == 0) {
      otui_app::showError("opentui_demo", "getNextBuffer failed");
      break;
    }

    // Ground truth: what OpenTUI is actually rendering this frame.
    const uint32_t buf_w = getBufferWidth(buffer);
    const uint32_t buf_h = getBufferHeight(buffer);
    if (buf_w > 0) {
      width = buf_w;
    }
    if (buf_h > 0) {
      height = buf_h;
    }

    bufferClear(buffer, bg);

    const uint32_t boxW = width > 10 ? width - 4 : width;
    const uint32_t boxH = height > 6 ? height - 4 : height;
    const int32_t boxX = 2;
    const int32_t boxY = 1;
    const char *title = " OpenTUI C++ ";
    const uint32_t packed = OTUI_BOX_ALL_SIDES | OTUI_BOX_FILL | OTUI_BOX_TITLE_CENTER;

    bufferDrawBox(buffer, boxX, boxY, boxW, boxH, otui_app::kRoundedBorder, packed, border, panelBg,
                  titleFg, reinterpret_cast<const uint8_t *>(title),
                  static_cast<uint32_t>(std::strlen(title)), nullptr, 0);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();
    const std::string line1 = "Native Zig core via C ABI";
    const std::string line2 = "Frames: " + std::to_string(frames);
    char elapsed_buf[64];
    std::snprintf(elapsed_buf, sizeof(elapsed_buf), "Elapsed: %.1fs", elapsed / 1000.0);
    const std::string line3 = elapsed_buf;
    const std::string line4 = "Size: " + std::to_string(width) + "x" + std::to_string(height);
    const char *hint = "Press q or Esc to quit  |  try opentui_procs.exe";

    const uint32_t textX = static_cast<uint32_t>(boxX + 3);
    otui_app::drawText(buffer, textX, static_cast<uint32_t>(boxY + 3), line1.c_str(), bodyFg, &panelBg,
                       OTUI_ATTR_NONE);
    otui_app::drawText(buffer, textX, static_cast<uint32_t>(boxY + 5), line2.c_str(), accent, &panelBg,
                       OTUI_ATTR_BOLD);
    otui_app::drawText(buffer, textX, static_cast<uint32_t>(boxY + 6), line3.c_str(), bodyFg, &panelBg,
                       OTUI_ATTR_NONE);
    otui_app::drawText(buffer, textX, static_cast<uint32_t>(boxY + 7), line4.c_str(), bodyFg, &panelBg,
                       OTUI_ATTR_DIM);
    otui_app::drawText(buffer, textX, static_cast<uint32_t>(boxY + boxH - 3), hint, titleFg, &panelBg,
                       OTUI_ATTR_UNDERLINE);

    render(renderer, true);
    ++frames;
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }

  destroyRenderer(renderer, true);
  return 0;
}
