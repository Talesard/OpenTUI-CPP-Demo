#include "opentui.h"
#include "otui_app.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <psapi.h>
#  include <tlhelp32.h>
#endif

namespace {

struct ProcessRow {
  uint32_t pid = 0;
  std::string name;
  uint64_t workingSet = 0;
};

std::string toLower(std::string s) {
  for (char &c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

std::string formatBytes(uint64_t bytes) {
  const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
  char buf[32];
  if (mb >= 100.0) {
    std::snprintf(buf, sizeof(buf), "%6.0f MB", mb);
  } else {
    std::snprintf(buf, sizeof(buf), "%6.1f MB", mb);
  }
  return buf;
}

std::string padRight(std::string s, size_t width) {
  if (s.size() > width) {
    if (width <= 3) {
      return s.substr(0, width);
    }
    return s.substr(0, width - 3) + "...";
  }
  s.append(width - s.size(), ' ');
  return s;
}

std::string padLeft(std::string s, size_t width) {
  if (s.size() >= width) {
    return s.substr(0, width);
  }
  return std::string(width - s.size(), ' ') + s;
}

std::vector<ProcessRow> snapshotProcesses() {
  std::vector<ProcessRow> rows;
#ifdef _WIN32
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) {
    return rows;
  }

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      ProcessRow row;
      row.pid = pe.th32ProcessID;
      char nameUtf8[MAX_PATH * 4]{};
      WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, nameUtf8, sizeof(nameUtf8), nullptr, nullptr);
      row.name = nameUtf8;

      HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, row.pid);
      if (proc) {
        PROCESS_MEMORY_COUNTERS_EX pmc{};
        if (GetProcessMemoryInfo(proc, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc))) {
          row.workingSet = pmc.WorkingSetSize;
        }
        CloseHandle(proc);
      }
      rows.push_back(std::move(row));
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
#endif

  std::sort(rows.begin(), rows.end(), [](const ProcessRow &a, const ProcessRow &b) {
    if (a.workingSet != b.workingSet) {
      return a.workingSet > b.workingSet;
    }
    return a.name < b.name;
  });
  return rows;
}

bool killProcess(uint32_t pid, std::string *error) {
#ifdef _WIN32
  if (pid == 0 || pid == GetCurrentProcessId()) {
    *error = "Refusing to kill this process";
    return false;
  }
  HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
  if (!proc) {
    *error = "Access denied (try elevated shell)";
    return false;
  }
  const BOOL ok = TerminateProcess(proc, 1);
  CloseHandle(proc);
  if (!ok) {
    *error = "TerminateProcess failed";
    return false;
  }
  return true;
#else
  *error = "Windows only";
  return false;
#endif
}

std::vector<size_t> filteredIndices(const std::vector<ProcessRow> &rows, const std::string &filter) {
  std::vector<size_t> out;
  const std::string needle = toLower(filter);
  out.reserve(rows.size());
  for (size_t i = 0; i < rows.size(); ++i) {
    if (needle.empty()) {
      out.push_back(i);
      continue;
    }
    const std::string hay = toLower(rows[i].name + " " + std::to_string(rows[i].pid));
    if (hay.find(needle) != std::string::npos) {
      out.push_back(i);
    }
  }
  return out;
}

bool isFilterTypeKey(const otui_app::KeyEvent &key) {
  if (key.ch < 32 || key.ch >= 127) {
    return false;
  }
  // Accept any printable ASCII that isn't a dedicated control chord.
  // Navigation is handled via virtualKey before this is consulted.
  return true;
}

}  // namespace

int main() {
  otui_app::prepareConsole();

  uint32_t width = 80;
  uint32_t height = 24;
  otui_app::consoleSize(&width, &height);
  width = (std::max)(width, 60u);
  height = (std::max)(height, 16u);

  const OtuiHandle renderer =
      createRenderer(width, height, OTUI_DEST_STDOUT, OTUI_REMOTE_LOCAL, nullptr);
  if (renderer == 0) {
    otui_app::showError("opentui_procs", "createRenderer failed. Is opentui.dll next to the exe?");
    return 1;
  }

  setUseThread(renderer, false);
  setupTerminal(renderer, true);
  otui_app::claimInput(renderer);

  OtuiRGBA bg{}, panel{}, border{}, titleFg{}, textFg{}, dimFg{}, selFg{}, selBg{}, accent{}, errFg{}, okFg{};
  otui_rgb(bg, 10, 12, 16, 255);
  otui_rgb(panel, 16, 20, 28, 255);
  otui_rgb(border, 70, 160, 220, 255);
  otui_rgb(titleFg, 255, 200, 90, 255);
  otui_rgb(textFg, 220, 228, 236, 255);
  otui_rgb(dimFg, 130, 140, 155, 255);
  otui_rgb(selFg, 10, 12, 16, 255);
  otui_rgb(selBg, 120, 210, 170, 255);
  otui_rgb(accent, 120, 210, 170, 255);
  otui_rgb(errFg, 255, 120, 120, 255);
  otui_rgb(okFg, 140, 230, 160, 255);
  setBackgroundColor(renderer, bg);

  constexpr const char *kHotkeys = "Up/Down  Del=kill  F5=refresh  Esc/q=quit";
  std::vector<ProcessRow> processes = snapshotProcesses();
  std::string filter;
  std::string toast;
  bool toastIsError = false;
  auto toastUntil = std::chrono::steady_clock::time_point{};
  int selected = 0;
  int scroll = 0;
  bool running = true;
  auto lastRefresh = std::chrono::steady_clock::now();

  const auto setToast = [&](std::string message, bool isError) {
    toast = std::move(message);
    toastIsError = isError;
    toastUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  };

  while (running) {
    if (otui_app::syncRendererSize(renderer, &width, &height, 40, 12)) {
      // Keep selection in view after the list height changes.
      scroll = (std::min)(scroll, selected);
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - lastRefresh > std::chrono::seconds(2)) {
      processes = snapshotProcesses();
      lastRefresh = now;
    }

    std::vector<size_t> visible = filteredIndices(processes, filter);
    if (selected >= static_cast<int>(visible.size())) {
      selected = (std::max)(0, static_cast<int>(visible.size()) - 1);
    }

    const otui_app::KeyEvent key = otui_app::pollKey();
    if (key.present) {
      if (key.virtualKey == VK_ESCAPE) {
        if (!filter.empty()) {
          filter.clear();
        } else {
          running = false;
          break;
        }
      } else if (key.ch == L'q' || key.ch == L'Q') {
        running = false;
        break;
      } else if (key.virtualKey == VK_F5) {
        processes = snapshotProcesses();
        lastRefresh = now;
        setToast("Refreshed", false);
      } else if (key.virtualKey == VK_UP) {
        selected = (std::max)(0, selected - 1);
      } else if (key.virtualKey == VK_DOWN) {
        if (!visible.empty()) {
          selected = (std::min)(static_cast<int>(visible.size()) - 1, selected + 1);
        }
      } else if (key.virtualKey == VK_PRIOR) {
        selected = (std::max)(0, selected - 10);
      } else if (key.virtualKey == VK_NEXT) {
        if (!visible.empty()) {
          selected = (std::min)(static_cast<int>(visible.size()) - 1, selected + 10);
        }
      } else if (key.virtualKey == VK_HOME) {
        selected = 0;
      } else if (key.virtualKey == VK_END) {
        selected = visible.empty() ? 0 : static_cast<int>(visible.size()) - 1;
      } else if (key.virtualKey == VK_BACK) {
        if (!filter.empty()) {
          filter.pop_back();
        }
      } else if (key.virtualKey == VK_DELETE) {
        if (!visible.empty()) {
          const ProcessRow &row = processes[visible[static_cast<size_t>(selected)]];
          std::string err;
          if (killProcess(row.pid, &err)) {
            setToast("Killed PID " + std::to_string(row.pid) + " (" + row.name + ")", false);
            processes = snapshotProcesses();
            lastRefresh = now;
          } else {
            setToast(std::move(err), true);
          }
        }
      } else if (!key.ctrl && isFilterTypeKey(key)) {
        filter.push_back(static_cast<char>(key.ch));
        selected = 0;
        scroll = 0;
      }

      visible = filteredIndices(processes, filter);
      if (selected >= static_cast<int>(visible.size())) {
        selected = (std::max)(0, static_cast<int>(visible.size()) - 1);
      }
    }

    const int boxX = 0;
    const int boxY = 0;
    const uint32_t boxW = width;
    const uint32_t boxH = height;
    // Interior rows: 1 .. height-2 (borders occupy 0 and height-1).
    const int listTop = 3;
    const int filterRow = static_cast<int>(height) - 3;
    const int statusRow = static_cast<int>(height) - 2;
    const int listBottom = filterRow - 1;
    const int viewRows = (std::max)(1, listBottom - listTop);
    if (selected < scroll) {
      scroll = selected;
    }
    if (selected >= scroll + viewRows) {
      scroll = selected - viewRows + 1;
    }

    const OtuiHandle buffer = getNextBuffer(renderer);
    if (buffer == 0) {
      break;
    }
    bufferClear(buffer, bg);

    const char *title = " procs ";
    bufferDrawBox(buffer, boxX, boxY, boxW, boxH, otui_app::kRoundedBorder,
                  OTUI_BOX_ALL_SIDES | OTUI_BOX_FILL | OTUI_BOX_TITLE_CENTER, border, panel, titleFg,
                  reinterpret_cast<const uint8_t *>(title), 7, nullptr, 0);

    const std::string header = padRight("PID", 8) + padRight("Mem", 10) + "Name";
    otui_app::drawText(buffer, 2, 1, header.c_str(), dimFg, &panel, OTUI_ATTR_UNDERLINE);

    const size_t maxLen = width > 4 ? static_cast<size_t>(width - 4) : 20;
    if (visible.empty()) {
      const char *emptyMsg = processes.empty() ? "No processes (snapshot failed)" : "No matches for filter";
      otui_app::drawText(buffer, 2, static_cast<uint32_t>(listTop), emptyMsg, errFg, &panel, OTUI_ATTR_BOLD);
    }
    for (int row = 0; row < viewRows; ++row) {
      const int idx = scroll + row;
      if (idx < 0 || idx >= static_cast<int>(visible.size())) {
        break;
      }
      const ProcessRow &proc = processes[visible[static_cast<size_t>(idx)]];
      const bool isSel = idx == selected;
      const std::string line =
          padLeft(std::to_string(proc.pid), 7) + " " + formatBytes(proc.workingSet) + "  " + proc.name;
      const std::string clipped = padRight(line, maxLen);
      otui_app::drawText(buffer, 2, static_cast<uint32_t>(listTop + row), clipped.c_str(),
                         isSel ? selFg : textFg, isSel ? &selBg : &panel,
                         isSel ? OTUI_ATTR_BOLD : OTUI_ATTR_NONE);
    }

    const std::string filterLine = "Filter: " + filter + "_";
    otui_app::drawText(buffer, 2, static_cast<uint32_t>(filterRow), padRight(filterLine, maxLen).c_str(),
                       accent, &panel, OTUI_ATTR_NONE);

    if (!toast.empty() && now >= toastUntil) {
      toast.clear();
    }
    const std::string counts =
        std::to_string(visible.size()) + "/" + std::to_string(processes.size());
    std::string footer = counts + "  " + kHotkeys;
    if (!toast.empty()) {
      footer = counts + "  " + toast + "  |  " + kHotkeys;
    }
    otui_app::drawText(buffer, 2, static_cast<uint32_t>(statusRow), padRight(footer, maxLen).c_str(),
                       toastIsError && !toast.empty() ? errFg : dimFg, &panel, OTUI_ATTR_DIM);

    render(renderer, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  destroyRenderer(renderer, true);
  return 0;
}
