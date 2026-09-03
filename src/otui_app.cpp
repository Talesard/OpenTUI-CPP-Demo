#include "otui_app.hpp"

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

namespace otui_app {
namespace {

#ifdef _WIN32
std::deque<KeyEvent> g_queue;
std::string g_esc;
DWORD g_escStarted = 0;
bool g_resizeHint = false;

void pushKey(unsigned vk, wchar_t ch = 0, bool ctrl = false) {
  KeyEvent ev{};
  ev.present = true;
  ev.keyDown = true;
  ev.virtualKey = vk;
  ev.ch = ch;
  ev.ctrl = ctrl;
  g_queue.push_back(ev);
}

bool endsWith(const std::string &s, const char *suffix) {
  const size_t n = std::strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool parseCsi(const std::string &seq) {
  if (seq.size() >= 3 && seq[0] == '\x1b' && seq[1] == '[') {
    const char last = seq.back();
    switch (last) {
      case 'A':
        pushKey(VK_UP);
        return true;
      case 'B':
        pushKey(VK_DOWN);
        return true;
      case 'C':
        pushKey(VK_RIGHT);
        return true;
      case 'D':
        pushKey(VK_LEFT);
        return true;
      case 'H':
        pushKey(VK_HOME);
        return true;
      case 'F':
        pushKey(VK_END);
        return true;
      case '~':
        if (endsWith(seq, "5~") && seq.find("[5") != std::string::npos) {
          pushKey(VK_PRIOR);
          return true;
        }
        if (endsWith(seq, "6~") && seq.find("[6") != std::string::npos) {
          pushKey(VK_NEXT);
          return true;
        }
        if (endsWith(seq, "3~") && seq.find("[3") != std::string::npos) {
          pushKey(VK_DELETE);
          return true;
        }
        {
          unsigned mods = 0;
          unsigned code = 0;
          if (std::sscanf(seq.c_str(), "\x1b[27;%u;%u~", &mods, &code) == 2 && code > 0 && code < 128) {
            pushKey(code, static_cast<wchar_t>(code), mods > 1);
            return true;
          }
        }
        return true;  // consume unknown CSI ~
      default:
        return true;  // consume finished CSI
    }
  }

  if (seq.size() == 3 && seq[0] == '\x1b' && seq[1] == 'O') {
    switch (seq[2]) {
      case 'A':
        pushKey(VK_UP);
        return true;
      case 'B':
        pushKey(VK_DOWN);
        return true;
      case 'C':
        pushKey(VK_RIGHT);
        return true;
      case 'D':
        pushKey(VK_LEFT);
        return true;
      case 'H':
        pushKey(VK_HOME);
        return true;
      case 'F':
        pushKey(VK_END);
        return true;
      default:
        return true;
    }
  }
  return false;
}

void feedByte(unsigned char byte, unsigned vkHint, bool ctrl) {
  if (!g_esc.empty()) {
    g_esc.push_back(static_cast<char>(byte));
    const char last = static_cast<char>(byte);
    const bool done = (last >= 0x40 && last <= 0x7E && g_esc.size() >= 2) || g_esc.size() > 48;
    if (done) {
      parseCsi(g_esc);
      g_esc.clear();
      g_escStarted = 0;
    }
    return;
  }

  if (byte == 0x1b || vkHint == VK_ESCAPE) {
    g_esc.assign(1, '\x1b');
    g_escStarted = GetTickCount();
    return;
  }

  if (vkHint == VK_BACK || byte == 0x7f || byte == 0x08) {
    pushKey(VK_BACK);
    return;
  }
  if (vkHint == VK_RETURN || byte == '\r' || byte == '\n') {
    pushKey(VK_RETURN);
    return;
  }
  if (vkHint == VK_TAB || byte == '\t') {
    pushKey(VK_TAB);
    return;
  }

  switch (vkHint) {
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_DELETE:
    case VK_INSERT:
    case VK_F5:
      pushKey(vkHint, 0, ctrl);
      return;
    default:
      break;
  }

  if (byte >= 32 && byte < 127) {
    pushKey(vkHint ? vkHint : byte, static_cast<wchar_t>(byte), ctrl);
  }
}

void ingestRecords() {
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  if (in == INVALID_HANDLE_VALUE) {
    return;
  }

  DWORD available = 0;
  if (!GetNumberOfConsoleInputEvents(in, &available) || available == 0) {
    return;
  }

  INPUT_RECORD records[64];
  DWORD read = 0;
  if (!ReadConsoleInputW(in, records, 64, &read)) {
    return;
  }

  for (DWORD i = 0; i < read; ++i) {
    if (records[i].EventType == WINDOW_BUFFER_SIZE_EVENT) {
      // dwSize is the buffer size; always re-query the visible window in syncRendererSize.
      g_resizeHint = true;
      continue;
    }
    if (records[i].EventType != KEY_EVENT || !records[i].Event.KeyEvent.bKeyDown) {
      continue;
    }
    const KEY_EVENT_RECORD &key = records[i].Event.KeyEvent;
    const bool ctrl = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    const unsigned vk = key.wVirtualKeyCode;
    const wchar_t ch = key.uChar.UnicodeChar;
    const WORD repeats = key.wRepeatCount ? key.wRepeatCount : 1;

    for (WORD r = 0; r < repeats; ++r) {
      if (ch != 0) {
        if (ch < 128) {
          feedByte(static_cast<unsigned char>(ch), vk, ctrl);
        }
      } else {
        switch (vk) {
          case VK_UP:
          case VK_DOWN:
          case VK_LEFT:
          case VK_RIGHT:
          case VK_HOME:
          case VK_END:
          case VK_PRIOR:
          case VK_NEXT:
          case VK_DELETE:
          case VK_BACK:
          case VK_F5:
          case VK_ESCAPE:
            if (vk == VK_ESCAPE) {
              feedByte(0x1b, VK_ESCAPE, ctrl);
            } else {
              pushKey(vk, 0, ctrl);
            }
            break;
          default:
            break;
        }
      }
    }
  }
}

void flushEscTimeout() {
  if (!g_esc.empty() && g_esc == "\x1b" && GetTickCount() - g_escStarted > 40) {
    pushKey(VK_ESCAPE);
    g_esc.clear();
    g_escStarted = 0;
  }
}

void configureClassicInput() {
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  if (in == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD mode = 0;
  if (!GetConsoleMode(in, &mode)) {
    return;
  }
  mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
  mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT |
            ENABLE_QUICK_EDIT_MODE | ENABLE_MOUSE_INPUT);
  SetConsoleMode(in, mode);
}
#endif

}  // namespace

void prepareConsole() {
#ifdef _WIN32
  if (!GetConsoleWindow()) {
    AllocConsole();
  }

  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {
      mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
      SetConsoleMode(out, mode);
    }
  }

  configureClassicInput();
  flushInput();
#endif
}

void showError(const char *title, const char *message) {
  std::fprintf(stderr, "%s: %s\n", title, message);
#ifdef _WIN32
  MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR);
#endif
}

bool consoleSize(uint32_t *width, uint32_t *height) {
#ifdef _WIN32
  // Prefer CONOUT$ — STD_OUTPUT_HANDLE can fail or lie if stdout is redirected.
  HANDLE out = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
  const bool close_handle = (out != INVALID_HANDLE_VALUE);
  if (!close_handle) {
    out = GetStdHandle(STD_OUTPUT_HANDLE);
  }

  if (out != INVALID_HANDLE_VALUE) {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(out, &info)) {
      // Visible window size (not the scrollback buffer height).
      int w = info.srWindow.Right - info.srWindow.Left + 1;
      int h = info.srWindow.Bottom - info.srWindow.Top + 1;

      // On Windows Terminal the buffer width usually matches the window; if the
      // window rect is stale/empty, fall back to buffer width + window height.
      if (w <= 1 && info.dwSize.X > 0) {
        w = info.dwSize.X;
      }
      if (h <= 1) {
        h = info.srWindow.Bottom - info.srWindow.Top + 1;
        if (h <= 1 && info.dwSize.Y > 0 && info.dwSize.Y < 200) {
          h = info.dwSize.Y;
        }
      }

      if (w > 0 && h > 0) {
        *width = static_cast<uint32_t>(w);
        *height = static_cast<uint32_t>(h);
        if (close_handle) {
          CloseHandle(out);
        }
        return true;
      }
    }
    if (close_handle) {
      CloseHandle(out);
    }
  }
#endif
  *width = 80;
  *height = 24;
  return false;
}

void flushInput() {
#ifdef _WIN32
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  if (in != INVALID_HANDLE_VALUE) {
    FlushConsoleInputBuffer(in);
  }
  g_queue.clear();
  g_esc.clear();
  g_escStarted = 0;
  g_resizeHint = false;
#endif
}

void drainInput(unsigned milliseconds) {
#ifdef _WIN32
  flushInput();
  const DWORD start = GetTickCount();
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  if (in == INVALID_HANDLE_VALUE) {
    return;
  }
  while (GetTickCount() - start < milliseconds) {
    DWORD available = 0;
    if (GetNumberOfConsoleInputEvents(in, &available) && available > 0) {
      INPUT_RECORD records[64];
      DWORD read = 0;
      ReadConsoleInputW(in, records, 64, &read);
    } else {
      Sleep(5);
    }
  }
  flushInput();
#else
  (void)milliseconds;
#endif
}

void claimInput(OtuiHandle renderer) {
#ifdef _WIN32
  setKittyKeyboardFlags(renderer, 0);
  disableKittyKeyboard(renderer);
  const char reset[] = "\x1b[>4;0m"
                       "\x1b[<u";
  writeOut(renderer, reinterpret_cast<const uint8_t *>(reset), sizeof(reset) - 1);

  configureClassicInput();
  drainInput(200);
  configureClassicInput();
#else
  (void)renderer;
#endif
}

KeyEvent pollKey() {
  KeyEvent ev{};
#ifdef _WIN32
  ingestRecords();
  flushEscTimeout();
  if (!g_queue.empty()) {
    ev = g_queue.front();
    g_queue.pop_front();
  }
#endif
  return ev;
}

bool syncRendererSize(OtuiHandle renderer, uint32_t *width, uint32_t *height, uint32_t min_width,
                      uint32_t min_height) {
  if (!width || !height || renderer == 0) {
    return false;
  }

#ifdef _WIN32
  ingestRecords();
  g_resizeHint = false;
#endif

  uint32_t next_w = *width;
  uint32_t next_h = *height;
  consoleSize(&next_w, &next_h);

  if (next_w < min_width) {
    next_w = min_width;
  }
  if (next_h < min_height) {
    next_h = min_height;
  }
  if (next_w < 1) {
    next_w = 1;
  }
  if (next_h < 1) {
    next_h = 1;
  }

  if (next_w == *width && next_h == *height) {
    return false;
  }

  resizeRenderer(renderer, next_w, next_h);
  *width = next_w;
  *height = next_h;
  return true;
}

}  // namespace otui_app