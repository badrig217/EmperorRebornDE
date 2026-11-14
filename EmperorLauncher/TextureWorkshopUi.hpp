#pragma once
// Grafik-Werkstatt: progress window for generating the HD textures (see TextureWorkshop.hpp).
// Modal: the launcher window is disabled while the worker thread runs. Cancel stops Real-ESRGAN at once.
#include <Windows.h>
#include <CommCtrl.h>
#include <chrono>
#include <string>
#include <thread>
#include "Lang.hpp"
#include "TextureWorkshop.hpp"

#pragma comment(lib, "comctl32.lib")

namespace hdTexturesUi
{
  struct State
  {
    hdTextures::Progress progress;
    HWND window = nullptr, label = nullptr, bar = nullptr, cancelButton = nullptr;
    std::chrono::steady_clock::time_point started, upscaleStarted;
    int upscaleStartDone = -1;
    bool finished = false;
  };
  inline State* gState = nullptr;

  inline std::wstring statusText(State& s)
  {
    const int stage = s.progress.stage, done = s.progress.done, total = s.progress.total;
    if (s.progress.cancel) return T(L"Cancelling…", L"Wird abgebrochen…");
    if (stage == hdTextures::StagePacking || stage == hdTextures::StageDone)
      return T(L"Packing the textures as a mod…", L"Texturen werden als Mod eingebunden…");
    if (stage != hdTextures::StageUpscaling || total == 0)
      return T(L"Reading the game files…", L"Spieldateien werden gelesen…");

    wchar_t line[256];
    swprintf(line, 256, T(L"AI upscaling: %d of %d textures", L"KI-Hochrechnung: %d von %d Texturen"), done, total);
    std::wstring text = line;
    // Time estimate from the speed since the first finished batch (the first batch includes start-up).
    auto now = std::chrono::steady_clock::now();
    if (s.upscaleStartDone < 0 && done > 0) { s.upscaleStartDone = done; s.upscaleStarted = now; }
    if (s.upscaleStartDone >= 0 && done > s.upscaleStartDone + 20)
    {
      double secs = std::chrono::duration<double>(now - s.upscaleStarted).count();
      int remaining = int(secs / (done - s.upscaleStartDone) * (total - done) / 60.0 + 0.999);
      swprintf(line, 256, T(L"\nabout %d min left", L"\nnoch etwa %d Min."), remaining < 1 ? 1 : remaining);
      text += line;
    }
    return text;
  }

  inline LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
  {
    State* s = gState;
    switch (msg)
    {
    case WM_TIMER:
      if (s)
      {
        int total = s->progress.total, done = s->progress.done;
        SendMessageW(s->bar, PBM_SETRANGE32, 0, total > 0 ? total : 1);
        SendMessageW(s->bar, PBM_SETPOS, total > 0 ? done : 0, 0);
        SetWindowTextW(s->label, statusText(*s).c_str());
      }
      return 0;
    case WM_COMMAND:
      if (s && reinterpret_cast<HWND>(lParam) == s->cancelButton && HIWORD(wParam) == BN_CLICKED)
      {
        s->progress.cancel = true;
        EnableWindow(s->cancelButton, FALSE);
      }
      return 0;
    case WM_CLOSE:
      if (s) { s->progress.cancel = true; EnableWindow(s->cancelButton, FALSE); }
      return 0; // the window closes when the worker has stopped
    case WM_CTLCOLORSTATIC:
      SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
      return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  // Runs the generation with a progress window. Returns L"" on success, L"cancelled" or an error text.
  inline std::wstring run(HWND owner, HFONT font, int (*scale)(int), const std::filesystem::path& toolDir,
                          const std::filesystem::path& gameDir)
  {
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);
    WNDCLASSW wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = reinterpret_cast<HICON>(SendMessageW(owner, WM_GETICON, ICON_BIG, 0));
    wc.lpszClassName = L"EmperorSpiceHdProgress";
    RegisterClassW(&wc);

    State state;
    gState = &state;
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT wr = { 0, 0, scale(420), scale(150) };
    AdjustWindowRectEx(&wr, style, FALSE, 0);
    RECT ownerRect;
    GetWindowRect(owner, &ownerRect);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    state.window = CreateWindowExW(0, wc.lpszClassName, T(L"Graphics workshop: HD textures", L"Grafik-Werkstatt: HD-Texturen"), style,
                                   (ownerRect.left + ownerRect.right - w) / 2, (ownerRect.top + ownerRect.bottom - h) / 2, w, h,
                                   owner, nullptr, wc.hInstance, nullptr);
    state.label = CreateWindowExW(0, WC_STATIC, T(L"Reading the game files…", L"Spieldateien werden gelesen…"), WS_CHILD | WS_VISIBLE,
                                  scale(16), scale(14), scale(388), scale(40), state.window, nullptr, nullptr, nullptr);
    state.bar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, scale(16), scale(60), scale(388), scale(20),
                                state.window, nullptr, nullptr, nullptr);
    state.cancelButton = CreateWindowExW(0, WC_BUTTON, T(L"Cancel", L"Abbrechen"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                         scale(304), scale(104), scale(100), scale(28), state.window, nullptr, nullptr, nullptr);
    HWND hint = CreateWindowExW(0, WC_STATIC, T(L"You can keep using your PC.", L"Du kannst den PC nebenbei weiter benutzen."), WS_CHILD | WS_VISIBLE,
                                scale(16), scale(110), scale(280), scale(20), state.window, nullptr, nullptr, nullptr);
    for (HWND c : { state.label, state.cancelButton, hint })
      SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    EnableWindow(owner, FALSE);
    ShowWindow(state.window, SW_SHOW);
    SetTimer(state.window, 1, 250, nullptr);
    state.started = std::chrono::steady_clock::now();

    std::wstring result;
    std::thread worker([&] {
      result = hdTextures::generate(toolDir, gameDir, state.progress);
      state.finished = true;
      PostMessageW(state.window, WM_NULL, 0, 0); // wake the loop below
    });
    MSG msg;
    while (!state.finished)
    {
      if (GetMessageW(&msg, nullptr, 0, 0) <= 0) // WM_QUIT: stop and pass it on
      {
        state.progress.cancel = true;
        PostQuitMessage(int(msg.wParam));
        break;
      }
      if (!IsDialogMessageW(state.window, &msg))
      {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
    worker.join();
    KillTimer(state.window, 1);
    EnableWindow(owner, TRUE);
    DestroyWindow(state.window);
    SetForegroundWindow(owner);
    gState = nullptr;
    return result;
  }
}
