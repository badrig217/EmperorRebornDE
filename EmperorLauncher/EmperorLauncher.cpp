#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#include <detours.h>
#include <cstdio>
#include <string>
#include <CommCtrl.h>
#include <map>
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

LRESULT onResolutionChanged(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int index = int(SendMessageA(resolutionCombo, CB_GETCURSEL, 0, 0));
  if (index >= 0 && index < int(std::size(resolutions)))
  {
    settings.screenWidth = resolutions[index].width;
    settings.screenHeight = resolutions[index].height;
    settings.widescreen = resolutions[index].widescreen;
    settings.upscaleToDesktop = resolutions[index].upscale;
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
  if (message == WM_CLOSE && hwnd == window) return OnWindowClose(hwnd, message, wParam, lParam);

  if (message == WM_COMMAND && HIWORD(wParam) == STN_CLICKED && reinterpret_cast<HWND>(lParam) == githubLink) return onLinkClicked(hwnd, message, wParam, lParam, "https://github.com/azmawee");
  if (message == WM_COMMAND && HIWORD(wParam) == STN_CLICKED && reinterpret_cast<HWND>(lParam) == facebookLink) return onLinkClicked(hwnd, message, wParam, lParam, "https://facebook.com/azmawee");

  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == fullscreenCheckbox) return onFullscreenCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == cutscene43Checkbox) return onCutscene43CheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == smoothScrollCheckbox) return onSmoothScrollCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == CBN_SELCHANGE && reinterpret_cast<HWND>(lParam) == languageCombo) return onLanguageChanged(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == CBN_SELCHANGE && reinterpret_cast<HWND>(lParam) == resolutionCombo) return onResolutionChanged(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == hostGameRadio) return onServerOrClientRadioClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == connectToServerRadio) return onServerOrClientRadioClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == EN_CHANGE && reinterpret_cast<HWND>(lParam) == serverAddressTextbox) return onServerAddressTextChanged(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == playButton) return OnPlayClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == pauseOnStartupCheckbox) return onPauseOnStartupCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == forceCursorVisibleCheckbox) return onForceCursorVisibleCheckboxClicked(hwnd, message, wParam, lParam);
  if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && reinterpret_cast<HWND>(lParam) == disableCursorCaptureCheckbox) return onDisableCursorCaptureCheckboxClicked(hwnd, message, wParam, lParam);
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


int wmain(int argc, wchar_t* argv[])
{
  std::wstring installDirUnicode;
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

    installEmperor(installDir);

    if (getMd5OfFile(installDir + "\\Game.exe").empty())
    {
      MessageBoxW(nullptr, T(L"Installation failed!", L"Installation fehlgeschlagen!"), T(L"Error", L"Fehler"), MB_OK);
      return 1;
    }
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
      installEmperor(installDir);
      if (getMd5OfFile(installDir + "\\Game.exe") != desiredMd5)
        MessageBoxW(nullptr, T(L"Game.exe still does not match the expected version - continuing anyway.",
                               L"Die Game.exe entspricht immer noch nicht der erwarteten Version - es geht trotzdem weiter."),
                    T(L"Note", L"Hinweis"), MB_OK);
    }
    // Yes: fall through and launch with the current Game.exe.
  }

  SetCurrentDirectoryA(installDir.c_str());

  // main window
  int width = 550;
  int height = 700;
  int left = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
  int top = GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2;
  window = CreateWindowEx(0, WC_DIALOG, L"Emperor Reborn", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, left, top, width, height, nullptr, nullptr, nullptr, nullptr);

  // Our own emblem (icon/EmperorReborn.ico, embedded via Resources.rc) on the title bar and taskbar.
  HICON appIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON));
  if (appIcon)
  {
    SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon));
    SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIcon));
  }

  // Branding header banner across the top (icon/launcher-header.bmp, embedded via Resources.rc).
  HWND headerPic = CreateWindowEx(0, WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_BITMAP, 12, 12, 514, 118, window, nullptr, nullptr, nullptr);
  HBITMAP headerBmp = reinterpret_cast<HBITMAP>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDB_HEADER), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
  if (headerBmp)
    SendMessageW(headerPic, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(headerBmp));

  int ySpace = 30;
  int headerH = 140;   // vertical space reserved for the header banner above the columns
  int yMax = 0;

  // left side
  {
    int y = ySpace + headerH;
    int x = 30;

    CreateWindowEx(0, WC_STATIC, T(L"Settings", L"Einstellungen"), WS_CHILD | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    fullscreenCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Fullscreen", L"Vollbild"), WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 140, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    CreateWindowEx(0, WC_STATIC, T(L"Resolution", L"Aufl\u00f6sung"), WS_CHILD | WS_VISIBLE, x, y + 4, 80, 24, window, nullptr, nullptr, nullptr);
    // The height here also controls how tall the drop-down list is, so make it big enough for all entries.
    resolutionCombo = CreateWindowEx(0, WC_COMBOBOX, nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, x + 90, y, 200, 240, window, nullptr, nullptr, nullptr);
    for (const Resolution& res : resolutions)
      SendMessageW(resolutionCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(res.label, res.labelDe)));
    // Make the drop-down list itself wider than the box so the longer "(widescreen)" labels are not clipped.
    SendMessageW(resolutionCombo, CB_SETDROPPEDWIDTH, 230, 0);
    y += ySpace;

    cutscene43Checkbox = CreateWindowEx(0, WC_BUTTON, T(L"Cutscene movies / FMV in 4:3 (black bars)", L"Zwischensequenzen in 4:3 (schwarze R\u00e4nder)"), WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 290, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    smoothScrollCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Smooth map scrolling", L"Fl\u00fcssiges Karten-Scrollen"), WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 290, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    y += ySpace;

    hostGameRadio = CreateWindowEx(0, WC_BUTTON, T(L"Host game / singleplayer", L"Spiel hosten / Einzelspieler"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    portsLabel = CreateWindowEx(0, WC_STATIC, T(L"Host must open port 4005 UDP and TCP", L"Host muss Port 4005 (UDP und TCP) freigeben"), WS_CHILD | WS_VISIBLE, x + 20, y, 300, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    connectToServerRadio = CreateWindowEx(0, WC_BUTTON, T(L"Connect to server", L"Mit Server verbinden"), WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    serverAddressLabel = CreateWindowEx(0, WC_STATIC, T(L"Server IP or hostname (dynamic DNS supported)", L"Server-IP oder Hostname (dynamisches DNS m\u00f6glich)"), WS_CHILD | WS_VISIBLE, x + 20, y, 320, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    serverAddressTextbox = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, L"host IP or hostname", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x + 20, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    testNetworkButton = CreateWindowEx(0, WC_BUTTON, T(L"Test connection", L"Verbindung testen"), WS_CHILD | WS_VISIBLE, x + 20, y, 140, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;


    yMax = std::max(yMax, y + 8);
  }

  // right side
  {
    int y = ySpace + headerH;
    int x = 330;

    CreateWindowEx(0, WC_STATIC, L"Sprache / Language", WS_CHILD | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    languageCombo = CreateWindowEx(0, WC_COMBOBOX, nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, x, y, 150, 120, window, nullptr, nullptr, nullptr);
    SendMessageW(languageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(languageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Deutsch"));
    SendMessageW(languageCombo, CB_SETCURSEL, gLang == Lang::DE ? 1 : 0, 0);
    y += ySpace;

    y += ySpace;

    CreateWindowEx(0, WC_STATIC, T(L"Debug settings", L"Fehlersuche"), WS_CHILD | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    pauseOnStartupCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Pause on startup", L"Beim Start pausieren"), WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    forceCursorVisibleCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Force cursor visible", L"Mauszeiger immer anzeigen"), WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;

    disableCursorCaptureCheckbox = CreateWindowEx(0, WC_BUTTON, T(L"Disable cursor capture", L"Maus nicht im Fenster fangen"), WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, x, y, 200, 24, window, nullptr, nullptr, nullptr);
    y += ySpace;


    yMax = std::max(yMax, y + 8);
  }

  playButton = CreateWindowEx(0, WC_BUTTON, T(L"Play", L"Spielen"), WS_CHILD | WS_VISIBLE, (width - 140) / 2, yMax, 140, 34, window, nullptr, nullptr, nullptr);

  // credits / author links (links clickable - open in browser)
  {
    int x = 30;
    int y = yMax + 50;

    CreateWindowEx(0, WC_STATIC, L"Emperor Reborn v2.5", WS_CHILD | WS_VISIBLE, x, y, 300, 20, window, nullptr, nullptr, nullptr);
    y += 24;

    githubLink = CreateWindowEx(0, WC_STATIC, L"github.com/azmawee", WS_CHILD | WS_VISIBLE | SS_NOTIFY, x, y, 300, 20, window, nullptr, nullptr, nullptr);
    y += 22;

    facebookLink = CreateWindowEx(0, WC_STATIC, L"facebook.com/azmawee", WS_CHILD | WS_VISIBLE | SS_NOTIFY, x, y, 300, 20, window, nullptr, nullptr, nullptr);
    y += 22;

    CreateWindowEx(0, WC_STATIC, L"azmawee@freebsd.my", WS_CHILD | WS_VISIBLE, x, y, 300, 20, window, nullptr, nullptr, nullptr);
  }

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
