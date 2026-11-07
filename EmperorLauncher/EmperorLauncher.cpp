#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#include <detours.h>
#include <cstdio>
#include <string>
#include <CommCtrl.h>
#include <map>
#include <algorithm>
#include <cwctype>
#include <optional>
#include <iterator>
#include "../EmperorHooks/Settings.hpp"
#include "../EmperorHooks/Error.hpp"
#include "Installer.hpp"
#include "Installer.hpp"
#include <filesystem>
#include "md5.h"
#include "NetworkTest.hpp"
#include "resource.h"
#include "Lang.hpp"
#include "GraphicsPack.hpp"
#include "FontPack.hpp"
#include "Shortcut.hpp"
#include "IsoReader.hpp"
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

// Modern Windows look for buttons, checkboxes and combo boxes (Common Controls v6). Without this the
// launcher's controls are drawn in the Windows 95 style.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


HANDLE globalCommsFileMappingHandle = nullptr;
HANDLE globalCommsHandleMutex = nullptr;

const char* globalCommsHandleMutexGuid = "48BC11BD-C4D7-466b-8A31-C6ABBAD47B3E";
const char* globalCommsEventNameGuid = "D6E7FC97-64F9-4d28-B52C-754EDF721C6F";
const char* decryptedEmperorDatData = "UIDATA,3DDATA,MAPS";


void closeGlobalCommsHandle()
{
  if (globalCommsFileMappingHandle)
  {
    CloseHandle(globalCommsFileMappingHandle);
    globalCommsFileMappingHandle = nullptr;
  }

  if (globalCommsHandleMutex)
  {
    CloseHandle(globalCommsHandleMutex);
    globalCommsHandleMutex = nullptr;
  }
}

void createGlobalCommsFileMappingHandle()
{
  closeGlobalCommsHandle();

  SetLastError(0);
  globalCommsHandleMutex = CreateMutexA(0, 0, globalCommsHandleMutexGuid);
  release_assert(globalCommsHandleMutex && GetLastError() == 0);

  SECURITY_ATTRIBUTES attributes = {};
  attributes.nLength = 12;
  attributes.lpSecurityDescriptor = 0;
  attributes.bInheritHandle = 1;

  SetLastError(0);
  globalCommsFileMappingHandle = CreateFileMappingA((HANDLE)0xFFFFFFFF, &attributes, 4u, 0, strlen(decryptedEmperorDatData) + 1, 0);
  release_assert(globalCommsFileMappingHandle && GetLastError() == 0);
}

std::wstring getFolderThisExeIsIn()
{
  std::wstring path;
  path.resize(4096);
  while (true)
  {
    SetLastError(0);
    DWORD size = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    DWORD err = GetLastError();

    if (err == ERROR_INSUFFICIENT_BUFFER)
    {
      path.resize(path.size() * 2);
      continue;
    }

    release_assert(err == 0);
    path.resize(size);
    break;
  }

  // Strip the exe filename (and the trailing separator) to get the containing folder.
  // Don't hardcode the exe name here - it breaks if the exe is renamed.
  size_t lastSlash = path.find_last_of(L"\\/");
  if (lastSlash != std::wstring::npos)
    path.resize(lastSlash);
  return path;
}

std::optional<std::string> wideToAscii(const std::wstring& wide)
{
  std::string ascii;
  ascii.resize(wide.size());
  for (size_t i = 0; i < wide.size(); i++)
  {
    if (wide[i] > 127)
      return std::nullopt;
    ascii[i] = char(wide[i]);
  }
  return ascii;
}

PROCESS_INFORMATION runGameExe()
{
  std::string hookDllPath = *wideToAscii(getFolderThisExeIsIn()) + "\\EmperorHooks.dll";
  std::string commandLine = "Game.exe -w";
  PROCESS_INFORMATION processInfo = {};

  STARTUPINFOA startupInfo = {};
  startupInfo.cb = sizeof(STARTUPINFOA);

  release_assert(DetourCreateProcessWithDllA(0, (char*)commandLine.c_str(), 0, 0, 1, 0, 0, 0, &startupInfo, &processInfo, hookDllPath.c_str(), nullptr));

  return processInfo;
}

void sendEmperorDatDataToGameProcessViaAnonymousMapping(HANDLE processHandle, DWORD idThread)
{
  void* commsMapping = MapViewOfFileEx(globalCommsFileMappingHandle, 0xF001Fu, 0, 0, 0, 0);
  release_assert(commsMapping);
  memcpy(commsMapping, decryptedEmperorDatData, strlen(decryptedEmperorDatData) + 1);
  UnmapViewOfFile(commsMapping);

  HANDLE gameReadyEvent = CreateEventA(0, 0, 0, globalCommsEventNameGuid);
  release_assert(gameReadyEvent && GetLastError() != ERROR_ALREADY_EXISTS);

  HANDLE Handles[2];
  Handles[0] = gameReadyEvent;
  Handles[1] = processHandle;
  if (!WaitForMultipleObjects(2u, Handles, 0, 0x493E0u) && globalCommsFileMappingHandle)
    PostThreadMessageA(idThread, 0xBEEFu, 0, (LPARAM)globalCommsFileMappingHandle);
  CloseHandle(gameReadyEvent);
}

void waitForExit(PROCESS_INFORMATION* gameRunData, DWORD* exitCode)
{
  WaitForSingleObject(gameRunData->hProcess, 0xFFFFFFFF);
  if (exitCode)
    *exitCode = -1;
  if (exitCode)
    GetExitCodeProcess(gameRunData->hProcess, exitCode);
}


void writeGraphicsSettings(int screenWidth, int screenHeight)
{
  auto setValue = [&](HKEY hkey, const char* subKey, const char* valueName, const std::string& value, bool force = false)
  {
    if (!force)
    {
      LSTATUS err = RegGetValueA(hkey, subKey, valueName, RRF_RT_REG_SZ, nullptr, nullptr, nullptr);
      if (err == ERROR_SUCCESS)
        return;
    }
    release_assert(RegSetKeyValueA(hkey, subKey, valueName, REG_SZ, value.c_str(), DWORD(value.size() + 1)) == ERROR_SUCCESS);
  };

  // force set width and height, the rest will we be set only if they don't exist
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "ScreenWidth", std::to_string(screenWidth), true);
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "ScreenHeight", std::to_string(screenHeight), true);

  // Just copied from the registry after cranking all settings in the in game settings dialogue
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "GraphicsLOD", "4");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "ColorDepth", "32");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "Shadows", "1");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "ModelLOD", "2");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "TextureLOD", "0");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "TerrainLOD", "2");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "EffectLOD", "2");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "MultiTexture", "1");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "HardwareTL", "1");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "AltDevice", "0");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "ShadowQuality", "2");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "LimitFrameRate", "0"); // this doesn't work, just btw
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Graphics", "LimitTo16BitTex", "0");

  // default to WOL when clicking multiplayer, don't offer LAN
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Multiplayer", "AutoLoginChoice", "NoAutoWolRadioButton");
  setValue(HKEY_CURRENT_USER, "Software\\WestwoodRedirect\\Emperor\\Options\\Multiplayer", "LastLoginNickname", "player");
}

int getMainMonitorHeight()
{
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  const POINT ptZero = { 0, 0 };
  HMONITOR monitor = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info = {};
  info.cbSize = sizeof(MONITORINFO);
  if (monitor && GetMonitorInfo(monitor, &info))
    return info.rcMonitor.bottom - info.rcMonitor.top;

  return GetSystemMetrics(SM_CYSCREEN); // fallback if the monitor query fails
}



// Selectable game resolutions. The 4:3 ones are the original ratio the game was built around.
// The "(widescreen)" ones are 16:9: picking one turns on the widescreen projection fix in the
// hooks so the battlefield renders properly wide instead of stretched. Width/height of 0 is a
// special "match the desktop" entry (the old behaviour: sharpest / fills the screen natively, but
// the in-game fonts get small on big monitors).
// The "Upscale ... big UI" entries keep the desktop at its native size and render at a lower 16:9
// resolution, stretching the frame up to fill the screen on present (like dgVoodoo). That keeps the
// in-game fonts large and readable on a big monitor without switching the display mode. These are the
// cleanest answer to "text too small at high resolution".
// The K tier is the render resolution, not the screen: a lower render gives a bigger (softer) UI, a
// higher render gives a sharper (smaller) UI. They are deliberately kept below true 4K, since rendering
// at the screen's own resolution would be a 1:1 present and bring the tiny fonts straight back. Pick the
// tier at or below your monitor: 4K render on a 4K screen would not upscale.
struct Resolution { int width; int height; bool widescreen; bool upscale; const wchar_t* label; const wchar_t* labelDe; };
const Resolution resolutions[] =
{
  { 640, 480, false, false, L"640 x 480", L"640 x 480" },
  { 800, 600, false, false, L"800 x 600", L"800 x 600" },
  { 1024, 768, false, false, L"1024 x 768", L"1024 x 768" },
  { 1152, 864, false, false, L"1152 x 864", L"1152 x 864" },
  { 1280, 720, true, false, L"1280 x 720 (widescreen)", L"1280 x 720 (Breitbild)" },
  { 1600, 900, true, false, L"1600 x 900 (widescreen)", L"1600 x 900 (Breitbild)" },
  { 1920, 1080, true, false, L"1920 x 1080 (widescreen)", L"1920 x 1080 (Breitbild)" },
  { 2560, 1440, true, false, L"2560 x 1440 (widescreen)", L"2560 x 1440 (Breitbild)" },
  { 2560, 1440, true, true, L"Upscale 4K, big UI", L"Hochskaliert 4K, gro\u00dfe UI" },
  { 1920, 1080, true, true, L"Upscale 2K, big UI", L"Hochskaliert 2K, gro\u00dfe UI" },
  { 1280, 720, true, true, L"Upscale 1K, big UI", L"Hochskaliert 1K, gro\u00dfe UI" },
  { 0, 0, false, false, L"Desktop", L"Desktop" },
};


WNDPROC defWndProc = nullptr;
HWND window = nullptr;

// --- Look & feel ---------------------------------------------------------------------------------
UINT gDpi = 96;
inline int S(int v) { return MulDiv(v, int(gDpi), 96); } // scale 96-DPI layout pixels to the system DPI

HFONT gUiFont = nullptr;
HFONT gLinkFont = nullptr;
int gHeaderBandHeight = 0;
HWND gHeaderPic = nullptr;
HBITMAP gHeaderBmp = nullptr;

void drawHeader(const DRAWITEMSTRUCT* item)
{
  if (!gHeaderBmp)
    return;
  BITMAP bm = {};
  GetObjectW(gHeaderBmp, sizeof(bm), &bm);
  HDC mem = CreateCompatibleDC(item->hDC);
  HGDIOBJ old = SelectObject(mem, gHeaderBmp);
  SetStretchBltMode(item->hDC, HALFTONE);
  SetBrushOrgEx(item->hDC, 0, 0, nullptr);
  const RECT& rc = item->rcItem;
  StretchBlt(item->hDC, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
  SelectObject(mem, old);
  DeleteDC(mem);
}
HBRUSH gBackgroundBrush = CreateSolidBrush(RGB(250, 249, 247));
const COLORREF gTextColor = RGB(32, 32, 32);
const COLORREF gLinkColor = RGB(180, 90, 20);

HFONT makeFont(int pointSize, int weight, bool underline = false)
{
  return CreateFontW(-MulDiv(pointSize, int(gDpi), 72), 0, 0, 0, weight, FALSE, underline, FALSE, DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

BOOL CALLBACK setFontOnChild(HWND child, LPARAM font)
{
  SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
  return TRUE;
}

// Light window body with a dark band behind the header banner (same gradient as the banner itself).
void paintBackground(HWND hwnd, HDC dc)
{
  RECT rc;
  GetClientRect(hwnd, &rc);
  FillRect(dc, &rc, gBackgroundBrush);
  for (int y = 0; y < gHeaderBandHeight; y++)
  {
    float t = float(y) / float(std::max(1, gHeaderBandHeight - 1));
    HBRUSH b = CreateSolidBrush(RGB(int(30 - 18 * t), int(20 - 11 * t), int(16 - 8 * t)));
    RECT line = { rc.left, y, rc.right, y + 1 };
    FillRect(dc, &line, b);
    DeleteObject(b);
  }
}

HWND fullscreenCheckbox = nullptr;
HWND resolutionCombo = nullptr;
HWND hostGameRadio = nullptr;
HWND portsLabel = nullptr;
HWND connectToServerRadio = nullptr;
HWND serverAddressLabel = nullptr;
HWND serverAddressTextbox = nullptr;

HWND playButton = nullptr;
HWND testNetworkButton = nullptr;

HWND githubLink = nullptr;
HWND facebookLink = nullptr;

HWND pauseOnStartupCheckbox = nullptr;
HWND forceCursorVisibleCheckbox = nullptr;
HWND disableCursorCaptureCheckbox = nullptr;
HWND cutscene43Checkbox = nullptr;
HWND smoothScrollCheckbox = nullptr;
HWND languageCombo = nullptr;
HWND graphicsPackCheckbox = nullptr;
HWND cameraZoomCombo = nullptr;

// Farthest camera zoom choices (percent of the original game's farthest zoom)
const int cameraZoomSteps[] = { 100, 125, 150, 175 };
HWND shortcutButton = nullptr;
HWND moneyCheatCheckbox = nullptr;
HWND largeFontCheckbox = nullptr;

// Command line argument (custom install dir) we were started with, so a language switch can restart us identically.
std::wstring gInstallDirArg;

bool playClicked = false;

Settings settings;


void refreshDerivedState()
{
  bool checked = SendMessage(connectToServerRadio, BM_GETCHECK, 0, 0) == BST_CHECKED;
  EnableWindow(serverAddressTextbox, checked);
  EnableWindow(testNetworkButton, checked);
}


void loadAndApplySettings()
{
  settings.readSettings();

  SendMessageA(fullscreenCheckbox, BM_SETCHECK, settings.fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);

  int resolutionIndex = 4; // default to 1280x720 (lowest widescreen) if the saved value isn't in the list
  for (int i = 0; i < int(std::size(resolutions)); i++)
  {
    // Match on upscale too, otherwise the native and "(upscale, big UI)" entries that share a
    // resolution (e.g. 1920x1080) are indistinguishable and the wrong one gets selected.
    if (resolutions[i].width == settings.screenWidth && resolutions[i].height == settings.screenHeight
        && resolutions[i].upscale == settings.upscaleToDesktop)
    {
      resolutionIndex = i;
      break;
    }
  }
  SendMessageA(resolutionCombo, CB_SETCURSEL, resolutionIndex, 0);

  SendMessageA(hostGameRadio, BM_SETCHECK, settings.hostGame ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(connectToServerRadio, BM_SETCHECK, settings.hostGame ? BST_UNCHECKED : BST_CHECKED, 0);
  SendMessageA(serverAddressTextbox, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(settings.serverAddress.c_str()));
  SendMessageA(pauseOnStartupCheckbox, BM_SETCHECK, settings.pauseOnStartup ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(forceCursorVisibleCheckbox, BM_SETCHECK, settings.forceCursorVisible ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(disableCursorCaptureCheckbox, BM_SETCHECK, settings.disableCursorCapture ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(cutscene43Checkbox, BM_SETCHECK, settings.cutscene43 ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(smoothScrollCheckbox, BM_SETCHECK, settings.smoothScroll ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(graphicsPackCheckbox, BM_SETCHECK, settings.graphicsPack ? BST_CHECKED : BST_UNCHECKED, 0);
  {
    int zoomIndex = 0;
    for (int i = 0; i < int(std::size(cameraZoomSteps)); i++)
      if (cameraZoomSteps[i] <= settings.cameraZoom)
        zoomIndex = i;
    SendMessageA(cameraZoomCombo, CB_SETCURSEL, zoomIndex, 0);
  }
  SendMessageA(moneyCheatCheckbox, BM_SETCHECK, settings.moneyCheat ? BST_CHECKED : BST_UNCHECKED, 0);
  SendMessageA(largeFontCheckbox, BM_SETCHECK, settings.largeFont ? BST_CHECKED : BST_UNCHECKED, 0);

  refreshDerivedState();
}


LRESULT onServerOrClientRadioClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  settings.hostGame = SendMessage(hostGameRadio, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();

  refreshDerivedState();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onFullscreenCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.fullscreen = SendMessage(fullscreenCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();

  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onCutscene43CheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.cutscene43 = SendMessage(cutscene43Checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();

  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onSmoothScrollCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.smoothScroll = SendMessage(smoothScrollCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();

  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onCameraZoomChanged(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int index = int(SendMessageA(cameraZoomCombo, CB_GETCURSEL, 0, 0));
  if (index >= 0 && index < int(std::size(cameraZoomSteps)))
  {
    settings.cameraZoom = cameraZoomSteps[index];
    settings.writeSettings();
  }
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onGraphicsPackCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.graphicsPack = SendMessage(graphicsPackCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onMoneyCheatCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.moneyCheat = SendMessage(moneyCheatCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onLargeFontCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.largeFont = SendMessage(largeFontCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onResolutionChanged(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int index = int(SendMessageA(resolutionCombo, CB_GETCURSEL, 0, 0));
  if (index >= 0 && index < int(std::size(resolutions)))
  {
    settings.screenWidth = resolutions[index].width;
    settings.screenHeight = resolutions[index].height;
    settings.widescreen = resolutions[index].widescreen;
    settings.upscaleToDesktop = resolutions[index].upscale;
    // The small in-game messages are drawn in fixed pixels, so they get tiny at high resolutions.
    // From 1440p (rendered natively, not upscaled) suggest the 2x message font. The player can still
    // change it afterwards.
    if (IsWindowEnabled(largeFontCheckbox))
    {
      settings.largeFont = !resolutions[index].upscale && resolutions[index].height >= 1440;
      SendMessage(largeFontCheckbox, BM_SETCHECK, settings.largeFont ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    settings.writeSettings();
  }
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

std::string GetText(HWND hwnd)
{
  size_t size = SendMessageA(hwnd, WM_GETTEXTLENGTH, 0, 0);
  if (!size)
    return "";
  std::string text(size, '\0');
  SendMessageA(hwnd, WM_GETTEXT, text.size() + 1, reinterpret_cast<LPARAM>(text.c_str()));
  return text;
}

LRESULT onServerAddressTextChanged(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.serverAddress = GetText(serverAddressTextbox);
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onPauseOnStartupCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.pauseOnStartup = SendMessage(pauseOnStartupCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onForceCursorVisibleCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.forceCursorVisible = SendMessage(forceCursorVisibleCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT onDisableCursorCaptureCheckboxClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  settings.disableCursorCapture = SendMessage(disableCursorCaptureCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  settings.writeSettings();
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

// Switching the language restarts the launcher, so every label is simply recreated in the new language.
LRESULT onLanguageChanged(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int index = int(SendMessageA(languageCombo, CB_GETCURSEL, 0, 0));
  Lang chosen = index == 1 ? Lang::DE : Lang::EN;
  if (chosen != gLang)
  {
    settings.language = languageToSetting(chosen);
    settings.writeSettings();

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring args = gInstallDirArg.empty() ? std::wstring() : (L"\"" + gInstallDirArg + L"\"");
    ShellExecuteW(nullptr, L"open", exePath, args.empty() ? nullptr : args.c_str(), nullptr, SW_SHOWNORMAL);

    DestroyWindow(window);
    PostQuitMessage(0);
    return 0;
  }
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT OnWindowClose(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  PostQuitMessage(0);
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT OnPlayClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  playClicked = true;
  DestroyWindow(window);
  PostQuitMessage(0);
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT OnShortcutClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (createDesktopShortcut())
    MessageBoxW(window, T(L"The shortcut \"Emperor Spice\" was created on your desktop.", L"Die Verkn\u00fcpfung \"Emperor Spice\" wurde auf dem Desktop angelegt."), L"Emperor Spice", MB_OK | MB_ICONINFORMATION);
  else
    MessageBoxW(window, T(L"The desktop shortcut could not be created.", L"Die Desktop-Verkn\u00fcpfung konnte nicht angelegt werden."), L"Emperor Spice", MB_OK | MB_ICONWARNING);
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT OnTestNetworkClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  testNetwork(window, settings.serverAddress);
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}


LRESULT onLinkClicked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, const char* url)
{
  ShellExecuteA(window, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (hwnd == window && message == WM_ERASEBKGND)
  {
    paintBackground(hwnd, reinterpret_cast<HDC>(wParam));
    return 1;
  }
  if (hwnd == window && (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN))
  {
    HDC dc = reinterpret_cast<HDC>(wParam);
    HWND control = reinterpret_cast<HWND>(lParam);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, (control == githubLink || control == facebookLink) ? gLinkColor : gTextColor);
    return reinterpret_cast<LRESULT>(gBackgroundBrush);
  }
  if (hwnd == window && message == WM_DRAWITEM && reinterpret_cast<const DRAWITEMSTRUCT*>(lParam)->hwndItem == gHeaderPic)
  {
    drawHeader(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
    return TRUE;
  }
  if (hwnd == window && message == WM_SETCURSOR)
  {
    HWND over = reinterpret_cast<HWND>(wParam);
    if (over == githubLink || over == facebookLink)
    {
      SetCursor(LoadCursor(nullptr, IDC_HAND));
      return TRUE;
    }
  }

  if (message == WM_CLOSE && hwnd == window) return OnWindowClose(hwnd, message, wParam, lParam);

  if (message == WM_COMMAND && HIWORD(wParam) == STN_CLICKED && reinterpret_cast<HWND>(lParam) == githubLink) return onLinkClicked(hwnd, message, wParam, lParam, "https://github.com/azmawee/EmperorReborn");
  if (message == WM_COMMAND && HIWORD(wParam) == STN_CLICKED && reinterpret_cast<HWND>(lParam) == facebookLink) return onLinkClicked(hwnd, message, wParam, lParam, "https://github.com/badrig217/EmperorSpice");

  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == fullscreenCheckbox) return onFullscreenCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == cutscene43Checkbox) return onCutscene43CheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == graphicsPackCheckbox) return onGraphicsPackCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == smoothScrollCheckbox) return onSmoothScrollCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == CBN_SELCHANGE && reinterpret_cast<HWND>(lParam) == languageCombo) return onLanguageChanged(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == CBN_SELCHANGE && reinterpret_cast<HWND>(lParam) == resolutionCombo) return onResolutionChanged(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == CBN_SELCHANGE && reinterpret_cast<HWND>(lParam) == cameraZoomCombo) return onCameraZoomChanged(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == hostGameRadio) return onServerOrClientRadioClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == connectToServerRadio) return onServerOrClientRadioClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == EN_CHANGE && reinterpret_cast<HWND>(lParam) == serverAddressTextbox) return onServerAddressTextChanged(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == playButton) return OnPlayClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == pauseOnStartupCheckbox) return onPauseOnStartupCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == forceCursorVisibleCheckbox) return onForceCursorVisibleCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == disableCursorCaptureCheckbox) return onDisableCursorCaptureCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == moneyCheatCheckbox) return onMoneyCheatCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == largeFontCheckbox) return onLargeFontCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == shortcutButton) return OnShortcutClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == testNetworkButton) return OnTestNetworkClicked(hwnd, message, wParam, lParam);

  return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

std::string getMd5OfFile(const std::string& path)
{
  FILE* f = fopen(path.c_str(), "rb");
  if (!f)
    return "";

  release_assert(fseek(f, 0, SEEK_END) == 0);
  long size = ftell(f);
  release_assert(size != -1);
  release_assert(fseek(f, 0, SEEK_SET) == 0);

  std::vector<uint8_t> data(size);
  release_assert(fread(data.data(), 1, size, f) == size);
  fclose(f);


  MD5_CTX md5 = {};
  MD5Init(&md5);
  MD5Update(&md5, data.data(), uint32_t(data.size()));
  unsigned char md5Bytes[16];
  MD5Final(md5Bytes, &md5);

  std::string md5Hex;
  md5Hex.resize(32);

  for (int32_t i = 0; i < 16; i++)
    sprintf(md5Hex.data() + i*2, "%02x", md5Bytes[i]);

  return md5Hex;
}


// Adds every .iso in the given folders whose volume label is one of the four Emperor discs and that is
// not assigned yet. So picking a single image is enough when all four lie in the same folder.
void addIsoImagesFromFolders(const std::vector<std::wstring>& files, std::map<std::string, std::wstring>& images)
{
  std::vector<std::filesystem::path> folders;
  for (const std::wstring& file : files)
  {
    std::filesystem::path folder = std::filesystem::path(file).parent_path();
    if (std::find(folders.begin(), folders.end(), folder) == folders.end())
      folders.push_back(folder);
  }
  for (const auto& folder : folders)
  {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec))
    {
      std::wstring ext = entry.path().extension().wstring();
      for (auto& c : ext) c = towlower(c);
      if (!entry.is_regular_file(ec) || ext != L".iso")
        continue;
      std::string label = iso::emperorDisc(entry.path());
      if (!label.empty() && !images.count(label))
        images[label] = entry.path().wstring();
    }
  }
}

// Lets the player pick the ISO images of the 4 discs. Each file is identified by its volume label
// (EMPEROR1..EMPEROR4), so the order and file names do not matter. Discs without an image are read
// from a CD drive later. Returns false if the player cancelled.
bool selectIsoImages(std::map<std::string, std::wstring>& images)
{
  while (true)
  {
    std::vector<wchar_t> buffer(64 * 1024, L'\0');
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"ISO (*.iso)\0*.iso\0*.*\0*.*\0\0";
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = DWORD(buffer.size());
    ofn.lpstrTitle = T(L"Select an ISO image of the game (the other discs in the same folder are found automatically)",
                       L"Ein ISO-Abbild des Spiels ausw\u00e4hlen (die anderen CDs im selben Ordner werden automatisch gefunden)");
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn))
      return false;

    // Multi-select result: "folder\0file1\0file2\0\0", single file: "full path\0\0".
    std::vector<std::wstring> files;
    std::wstring first = buffer.data();
    const wchar_t* p = buffer.data() + first.size() + 1;
    if (*p == L'\0')
      files.push_back(first);
    else
      for (; *p; p += wcslen(p) + 1)
        files.push_back(first + L"\\" + p);

    images.clear();
    std::wstring unknown;
    for (const std::wstring& file : files)
    {
      std::string label = iso::emperorDisc(std::filesystem::path(file));
      if (!label.empty())
        images[label] = file;
      else
        unknown += L"\n  " + std::filesystem::path(file).filename().wstring();
    }
    addIsoImagesFromFolders(files, images);

    std::wstring summary;
    for (int i = 1; i <= 4; i++)
    {
      std::string label = "EMPEROR" + std::to_string(i);
      auto it = images.find(label);
      summary += (gLang == Lang::DE ? L"\nCD " : L"\nDisc ") + std::to_wstring(i) + L": " +
                 (it != images.end() ? std::filesystem::path(it->second).filename().wstring()
                                     : std::wstring(T(L"- (will be asked for as a CD)", L"- (wird als CD abgefragt)")));
    }
    if (!unknown.empty())
      summary += std::wstring(T(L"\n\nNot an Emperor disc image:", L"\n\nKein Emperor-CD-Abbild:")) + unknown;

    int choice = MessageBoxW(nullptr, (std::wstring(T(L"Found these disc images:\n", L"Gefundene CD-Abbilder:\n")) + summary +
                                       T(L"\n\nContinue = install,  Try Again = choose again,  Cancel = quit",
                                         L"\n\nWeiter = installieren,  Wiederholen = neu ausw\u00e4hlen,  Abbrechen = beenden")).c_str(),
                             L"Emperor Spice", (images.empty() ? MB_RETRYCANCEL : MB_CANCELTRYCONTINUE) | MB_ICONQUESTION);
    if (choice == IDCANCEL)
      return false;
    if (choice == IDCONTINUE && !images.empty())
      return true;
    // IDRETRY / IDTRYAGAIN: pick again
  }
}

// Asks how to install: from CDs (or images already mounted in Windows) or from ISO files.
// Returns false if the player cancelled; isoImages is filled when ISO files were chosen.
bool chooseInstallSource(std::map<std::string, std::wstring>& isoImages)
{
  int choice = MessageBoxW(nullptr, T(
      L"How do you want to install the game?\n\n"
      L"Yes - from the CDs (or disc images mounted in Windows)\n"
      L"No - from ISO files (no mounting needed)\n"
      L"Cancel - quit",
      L"Wie m\u00f6chtest du das Spiel installieren?\n\n"
      L"Ja - von den CDs (oder in Windows eingebundenen Abbildern)\n"
      L"Nein - aus ISO-Dateien (ohne Einbinden)\n"
      L"Abbrechen - beenden"),
      L"Emperor Spice", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (choice == IDCANCEL)
    return false;
  isoImages.clear();
  if (choice == IDNO)
    return selectIsoImages(isoImages);
  return true;
}

int wmain(int argc, wchar_t* argv[])
{
  std::wstring installDirUnicode;
  // Crisp rendering on scaled displays: lay out in real pixels for the system DPI.
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
  gDpi = GetDpiForSystem();

  // Pick the UI language first, so every message from here on (installer included) is translated.
  settings.readSettings();
  gLang = languageFromSetting(settings.language);

  if (argc > 1)
  {
    installDirUnicode = argv[1];
    gInstallDirArg = argv[1];
  }
  else
    installDirUnicode = getFolderThisExeIsIn() + L"\\data";

  // This is unfortunate, but I just don't want to deal with debugging it.
  // Pretty sure the base game won't work anyway, even if I fix my code.
  for (std::wstring item : {installDirUnicode, getFolderThisExeIsIn()})
  {
    if (!wideToAscii(item).has_value())
    {
      MessageBoxW(nullptr, (item + T(L" contains non-English characters (e.g. umlauts), please move to a folder with only English characters",
                                       L" enth\u00e4lt Sonderzeichen (z. B. Umlaute). Bitte verschiebe alles in einen Ordner, dessen Pfad nur englische Buchstaben enth\u00e4lt.")).c_str(),
                  T(L"Bad install path", L"Ung\u00fcltiger Installationspfad"), MB_OK);
      return 1;
    }
  }

  std::string installDir = *wideToAscii(installDirUnicode);
  std::string gameExeMd5 = getMd5OfFile(installDir + "\\Game.exe");
  std::string_view desiredMd5 = "7dc4fda2f6b7e8a6b70e846686a81938";

  if (gameExeMd5.empty())
  {
    // No Game.exe at all - nothing to run, so we must install (or quit).
    if (MessageBoxW(nullptr, (T(L"Game.exe was not found in ", L"Game.exe wurde nicht gefunden in ") + toWide(installDir) +
                              T(L".\n\nInstall the game here now?", L".\n\nSoll das Spiel jetzt hier installiert werden?")).c_str(),
                    T(L"Game not found", L"Spiel nicht gefunden"), MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
      return 0;

    std::map<std::string, std::wstring> isoImages;
    if (!chooseInstallSource(isoImages))
      return 0;

    installEmperor(installDir, isoImages);

    if (getMd5OfFile(installDir + "\\Game.exe").empty())
    {
      MessageBoxW(nullptr, T(L"Installation failed!", L"Installation fehlgeschlagen!"), T(L"Error", L"Fehler"), MB_OK);
      return 1;
    }

    bool shortcut = createDesktopShortcut();
    MessageBoxW(nullptr, shortcut
                  ? T(L"Installation complete. A shortcut \"Emperor Spice\" was created on your desktop.",
                      L"Installation abgeschlossen. Auf dem Desktop wurde die Verkn\u00fcpfung \"Emperor Spice\" angelegt.")
                  : T(L"Installation complete.", L"Installation abgeschlossen."),
                L"Emperor Spice", MB_OK | MB_ICONINFORMATION);
  }
  else if (gameExeMd5 != desiredMd5)
  {
    // Game.exe exists but is not the original v1.09 build the patches are written for (e.g. another
    // mod's patched exe, or a different patch level). Warn, but let the user run anyway if they want.
    int choice = MessageBoxW(nullptr, T(
        L"Your Game.exe is not the original v1.09 version this launcher's features are built for "
        L"(checksum mismatch - it may be from another mod or a different patch).\n\n"
        L"Widescreen and the other tweaks may not work and the game could crash or be unstable. "
        L"You can proceed at your own risk.\n\n"
        L"Yes - run anyway (at your own risk)\n"
        L"No - (re)install the correct Game.exe\n"
        L"Cancel - quit",
        L"Deine Game.exe ist nicht die originale Version 1.09, f\u00fcr die dieser Launcher gebaut ist "
        L"(Pr\u00fcfsumme stimmt nicht - evtl. von einer anderen Mod oder einem anderen Patch).\n\n"
        L"Breitbild und die anderen Verbesserungen funktionieren dann eventuell nicht, das Spiel kann abst\u00fcrzen. "
        L"Du kannst auf eigenes Risiko fortfahren.\n\n"
        L"Ja - trotzdem starten (auf eigenes Risiko)\n"
        L"Nein - die richtige Game.exe (neu) installieren\n"
        L"Abbrechen - beenden"),
        T(L"Game.exe version mismatch", L"Falsche Game.exe-Version"), MB_YESNOCANCEL | MB_ICONWARNING);

    if (choice == IDCANCEL)
      return 0;
    if (choice == IDNO)
    {
      std::map<std::string, std::wstring> isoImages;
      if (!chooseInstallSource(isoImages))
        return 0;
      installEmperor(installDir, isoImages);
      if (getMd5OfFile(installDir + "\\Game.exe") != desiredMd5)
        MessageBoxW(nullptr, T(L"Game.exe still does not match the expected version - continuing anyway.",
                               L"Die Game.exe entspricht immer noch nicht der erwarteten Version - es geht trotzdem weiter."),
                    T(L"Note", L"Hinweis"), MB_OK);
    }
    // Yes: fall through and launch with the current Game.exe.
  }

  SetCurrentDirectoryA(installDir.c_str());

  // main window
  // Layout is written in 96-DPI pixels and scaled with S() for the system DPI, so the launcher stays
  // crisp (not bitmap-stretched) on displays with Windows scaling above 100%.
  const int clientW = S(560);
  const int y0 = S(158);          // below the header band
  const int colL = S(16), colLW = S(330);
  const int colR = S(356), colRW = S(188);
  const int rowH = S(26);

  RECT wr = { 0, 0, clientW, S(600) };
  window = CreateWindowEx(0, WC_DIALOG, L"Emperor Spice", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                          0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);

  // Our own emblem (icon/EmperorSpice.ico, embedded via Resources.rc) on the title bar and taskbar.
  HICON appIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON));
  if (appIcon)
  {
    SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon));
    SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIcon));
  }

  // Branding header banner (icon/launcher-header.bmp, generated by icon/generate_spice.py).
  // The bitmap is stored at 2x size and drawn scaled down with HALFTONE filtering (see WM_DRAWITEM),
  // so the banner stays sharp at any Windows display scaling.
  gHeaderPic = CreateWindowEx(0, WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                              (clientW - S(514)) / 2, S(14), S(514), S(118), window, nullptr, nullptr, nullptr);
  gHeaderBmp = reinterpret_cast<HBITMAP>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDB_HEADER), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));

  auto groupBox = [&](const wchar_t* text, int x, int y, int w, int h)
  {
    return CreateWindowEx(0, WC_BUTTON, text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, x, y, w, h, window, nullptr, nullptr, nullptr);
  };

  // --- Graphics (left) ---
  {
    const int h = S(236);
    groupBox(T(L"Graphics", L"Grafik"), colL, y0, colLW, h);
    int x = colL + S(14);
    int y = y0 + S(24);

    fullscreenCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Fullscreen", L"Vollbild"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, S(290), S(22), window, nullptr, nullptr, nullptr);
    y += rowH + S(4);

    CreateWindowEx(0, WC_STATIC, T(L"Resolution", L"Aufl\u00f6sung"), WS_CHILD | WS_VISIBLE, x, y + S(4), S(90), S(22), window, nullptr, nullptr, nullptr);
    // The height here also controls how tall the drop-down list is, so make it big enough for all entries.
    resolutionCombo = CreateWindowEx(0, WC_COMBOBOX, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, x + S(96), y, S(206), S(320), window, nullptr, nullptr, nullptr);
    for (const Resolution& res : resolutions)
      SendMessageW(resolutionCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(res.label, res.labelDe)));
    SendMessageW(resolutionCombo, CB_SETDROPPEDWIDTH, S(240), 0);
    y += rowH + S(10);

    cutscene43Checkbox = CreateWindowEx(0, WC_BUTTON, T(L"Cutscene movies / FMV in 4:3 (black bars)", L"Zwischensequenzen in 4:3 (schwarze R\u00e4nder)"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, S(305), S(22), window, nullptr, nullptr, nullptr);
    y += rowH;

    smoothScrollCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Smooth map scrolling", L"Fl\u00fcssiges Karten-Scrollen"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, S(305), S(22), window, nullptr, nullptr, nullptr);
    y += rowH;

    // Greyed out when the launcher was built without the bundled graphics folder (developer builds).
    graphicsPackCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Graphics pack (dgVoodoo + ReShade)", L"Grafik-Paket (dgVoodoo + ReShade)"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, S(305), S(22), window, nullptr, nullptr, nullptr);
    if (!graphicsPack::available(std::filesystem::path(getFolderThisExeIsIn()) / L"graphics"))
      EnableWindow(graphicsPackCheckbox, FALSE);
    y += rowH + S(6);

    CreateWindowEx(0, WC_STATIC, T(L"Camera zoom", L"Kamera-Zoom"), WS_CHILD | WS_VISIBLE, x, y + S(4), S(90), S(22), window, nullptr, nullptr, nullptr);
    cameraZoomCombo = CreateWindowEx(0, WC_COMBOBOX, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, x + S(96), y, S(206), S(120), window, nullptr, nullptr, nullptr);
    SendMessageW(cameraZoomCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(L"Original", L"Original")));
    SendMessageW(cameraZoomCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(L"A little farther (1.25x)", L"Etwas weiter (1,25x)")));
    SendMessageW(cameraZoomCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(L"Farther (1.5x, recommended)", L"Weiter (1,5x, empfohlen)")));
    SendMessageW(cameraZoomCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(L"Far (1.75x)", L"Sehr weit (1,75x)")));
  }

  // --- Language + Advanced (right) ---
  {
    groupBox(L"Sprache / Language", colR, y0, colRW, S(60));
    languageCombo = CreateWindowEx(0, WC_COMBOBOX, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, colR + S(14), y0 + S(24), colRW - S(28), S(120), window, nullptr, nullptr, nullptr);
    SendMessageW(languageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(languageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Deutsch"));
    SendMessageW(languageCombo, CB_SETCURSEL, gLang == Lang::DE ? 1 : 0, 0);

    int gy = y0 + S(70);
    groupBox(T(L"Troubleshooting", L"Fehlersuche"), colR, gy, colRW, S(166));
    int x = colR + S(14);
    int y = gy + S(24);
    pauseOnStartupCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Pause on startup", L"Beim Start pausieren"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, colRW - S(24), S(22), window, nullptr, nullptr, nullptr);
    y += rowH;
    forceCursorVisibleCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Force cursor visible", L"Mauszeiger immer zeigen"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, colRW - S(24), S(22), window, nullptr, nullptr, nullptr);
    y += rowH;
    disableCursorCaptureCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Disable cursor capture", L"Maus nicht einfangen"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, x, y, colRW - S(24), S(22), window, nullptr, nullptr, nullptr);
  }

  // --- Multiplayer (full width) ---
  int mpY = y0 + S(248);
  {
    const int h = S(132);
    groupBox(L"Multiplayer", colL, mpY, clientW - S(32), h);
    int x = colL + S(14);
    int y = mpY + S(24);

    hostGameRadio = CreateWindowEx(0, WC_BUTTON, T(L"Host game / singleplayer", L"Spiel hosten / Einzelspieler"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP, x, y, S(240), S(22), window, nullptr, nullptr, nullptr);
    portsLabel = CreateWindowEx(0, WC_STATIC, T(L"Host must open port 4005 UDP and TCP", L"Host muss Port 4005 (UDP + TCP) freigeben"), WS_CHILD | WS_VISIBLE, x + S(20), y + S(24), S(230), S(36), window, nullptr, nullptr, nullptr);
    connectToServerRadio = CreateWindowEx(0, WC_BUTTON, T(L"Connect to server", L"Mit Server verbinden"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON, x, y + S(70), S(240), S(22), window, nullptr, nullptr, nullptr);

    int rx = colL + S(270);
    serverAddressLabel = CreateWindowEx(0, WC_STATIC, T(L"Server IP or hostname (dynamic DNS supported)", L"Server-IP oder Hostname (auch dynamisches DNS)"), WS_CHILD | WS_VISIBLE, rx, y, S(244), S(36), window, nullptr, nullptr, nullptr);
    serverAddressTextbox = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, L"host IP or hostname", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, rx, y + S(38), S(244), S(24), window, nullptr, nullptr, nullptr);
    testNetworkButton = CreateWindowEx(0, WC_BUTTON, T(L"Test connection", L"Verbindung testen"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, rx, y + S(68), S(150), S(28), window, nullptr, nullptr, nullptr);
  }

  // --- Extras (full width) ---
  int exY = mpY + S(144);
  {
    groupBox(L"Extras", colL, exY, clientW - S(32), S(126));
    moneyCheatCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Money cheat in single player:  Ctrl+Shift+G = +25,000 credits",
                                                        L"Geld-Cheat im Einzelspieler:  Strg+Umschalt+G = +25.000 Credits"),
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, colL + S(14), exY + S(24), S(490), S(22), window, nullptr, nullptr, nullptr);
    // Greyed out when the launcher was built without the bundled fonts folder (developer builds).
    largeFontCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Large in-game messages and hints (2x)",
                                                       L"Gro\u00dfe Meldungen und Hinweise im Spiel (2x)"),
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, colL + S(14), exY + S(50), S(490), S(22), window, nullptr, nullptr, nullptr);
    if (!fontPack::available(std::filesystem::path(getFolderThisExeIsIn()) / L"fonts"))
      EnableWindow(largeFontCheckbox, FALSE);
    shortcutButton = CreateWindowEx(0, WC_BUTTON, T(L"Create desktop shortcut", L"Desktop-Verkn\u00fcpfung erstellen"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, colL + S(14), exY + S(84), S(230), S(28), window, nullptr, nullptr, nullptr);
  }

  // --- Play button ---
  int playY = exY + S(140);
  playButton = CreateWindowEx(0, WC_BUTTON, T(L"Play", L"Spielen"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, (clientW - S(200)) / 2, playY, S(200), S(42), window, nullptr, nullptr, nullptr);

  // --- Footer: version + credits (links open in the browser) ---
  int footY = playY + S(58);
  HWND versionLabel = CreateWindowEx(0, WC_STATIC, L"Emperor Spice v1.0", WS_CHILD | WS_VISIBLE, colL, footY, S(200), S(20), window, nullptr, nullptr, nullptr);
  githubLink = CreateWindowEx(0, WC_STATIC, T(L"Based on Emperor Reborn by azmawee", L"Basiert auf Emperor Reborn von azmawee"), WS_CHILD | WS_VISIBLE | SS_NOTIFY, colL, footY + S(22), S(330), S(20), window, nullptr, nullptr, nullptr);
  facebookLink = CreateWindowEx(0, WC_STATIC, L"github.com/badrig217/EmperorSpice", WS_CHILD | WS_VISIBLE | SS_NOTIFY, colL, footY + S(42), S(330), S(20), window, nullptr, nullptr, nullptr);
  int clientH = footY + S(70);

  // Fonts: Segoe UI everywhere, semibold for the play button and the version line.
  gUiFont = makeFont(9, FW_NORMAL);
  gLinkFont = makeFont(9, FW_NORMAL, true);
  HFONT boldFont = makeFont(9, FW_SEMIBOLD);
  HFONT playFont = makeFont(12, FW_SEMIBOLD);
  EnumChildWindows(window, setFontOnChild, reinterpret_cast<LPARAM>(gUiFont));
  SendMessageW(playButton, WM_SETFONT, reinterpret_cast<WPARAM>(playFont), TRUE);
  SendMessageW(versionLabel, WM_SETFONT, reinterpret_cast<WPARAM>(boldFont), TRUE);
  SendMessageW(githubLink, WM_SETFONT, reinterpret_cast<WPARAM>(gLinkFont), TRUE);
  SendMessageW(facebookLink, WM_SETFONT, reinterpret_cast<WPARAM>(gLinkFont), TRUE);

  // Size the window so its client area fits the layout, centred on the screen.
  gHeaderBandHeight = S(146);
  wr = { 0, 0, clientW, clientH };
  AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);
  int winW = wr.right - wr.left, winH = wr.bottom - wr.top;
  SetWindowPos(window, nullptr, (GetSystemMetrics(SM_CXSCREEN) - winW) / 2, (GetSystemMetrics(SM_CYSCREEN) - winH) / 2, winW, winH, SWP_NOZORDER);

  defWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

  loadAndApplySettings();

  ShowWindow(window, SW_SHOW);

  MSG message = { 0 };
  while (GetMessage(&message, nullptr, 0, 0))
  {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  if (playClicked)
  {
    int gameWidth = settings.screenWidth;
    int gameHeight = settings.screenHeight;

    if (gameWidth <= 0 || gameHeight <= 0)
    {
      // "Desktop (match screen)" was selected - match the monitor like the original behaviour.
      gameHeight = getMainMonitorHeight();

      if (!settings.fullscreen)
        gameHeight = std::max(int(gameHeight * 0.8), 600);

      // Lots of stuff in the game assumes a 4:3 screen ratio.
      gameWidth = int(float(gameHeight) * 4.0f / 3.0f);
    }

    // The fixed presets are all 4:3, which the game expects. Picking a low one avoids locking to
    // the (often very high) desktop resolution, which makes the in-game fonts unreadably small.
    writeGraphicsSettings(gameWidth, gameHeight);

    // Install or remove the graphics pack (dgVoodoo2 + ReShade) in the game folder.
    std::wstring packError = graphicsPack::apply(std::filesystem::path(getFolderThisExeIsIn()) / L"graphics",
                                                 std::filesystem::path(installDirUnicode), settings.graphicsPack);
    if (!packError.empty())
      MessageBoxW(nullptr, (T(L"The graphics pack could not be set up - the game starts without it.\n\n",
                              L"Das Grafik-Paket konnte nicht eingerichtet werden - das Spiel startet ohne.\n\n") + packError).c_str(),
                  L"Emperor Spice", MB_OK | MB_ICONWARNING);

    // Large in-game text: make sure the HD font archives are in the DATA folder (EmperorHooks switches
    // the fonts only when it finds them, so a failure here just means the original fonts).
    if (settings.largeFont)
    {
      std::wstring fontError = fontPack::install(std::filesystem::path(getFolderThisExeIsIn()) / L"fonts",
                                                 std::filesystem::path(installDirUnicode) / L"DATA");
      if (!fontError.empty())
        MessageBoxW(nullptr, (T(L"The large fonts could not be installed - the game starts with the normal text size.\n\n",
                                L"Die gro\u00dfe Schrift konnte nicht eingerichtet werden - das Spiel startet mit normaler Schrift.\n\n") + fontError).c_str(),
                    L"Emperor Spice", MB_OK | MB_ICONWARNING);
    }

    createGlobalCommsFileMappingHandle();

    PROCESS_INFORMATION runGameData = runGameExe();

    sendEmperorDatDataToGameProcessViaAnonymousMapping(runGameData.hProcess, runGameData.dwThreadId);

    DWORD ExitCode = 0;
    waitForExit(&runGameData, &ExitCode);

    closeGlobalCommsHandle();

    return ExitCode;
  }

  return 0;
}
