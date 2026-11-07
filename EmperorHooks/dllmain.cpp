#include <windows.h>
#include <detours.h>
#include "HookD3D7.hpp"
#include "PatchD3D7ResolutionLimit.hpp"
#include "GameExeImports.hpp"
#include "DoCdCheck.hpp"
#include "PatchDebugLog.hpp"
#include "PatchRegistry.hpp"
#include "PatchWindowManagement.hpp"
#include "PickingTracer.hpp"
#include "InGameHudFix.hpp"
#include "MovieFix.hpp"
#include "SmoothScroll.hpp"
#include "WrapWinsock.hpp"
#include "Log.hpp"
#include "PatchWol.hpp"
#include "Error.hpp"
#include <optional>
#include "Settings.hpp"


int (WINAPI* TrueShowCursor)(BOOL bShow) = ShowCursor;
VOID(WINAPI* OutputDebugStringAReal) (_In_opt_ LPCSTR lpOutputString) = OutputDebugStringA;
VOID(WINAPI* OutputDebugStringWReal) (_In_opt_ LPCWSTR lpOutputString) = OutputDebugStringW;


VOID WINAPI OutputDebugStringAWrap(_In_opt_ LPCSTR lpOutputString)
{
  // intel gpu drivers spam this for some reason
  if (lpOutputString && strncmp(lpOutputString, "Peiran message result", 21) == 0)
    return;

  Log("OutputDebugStringA: %s", lpOutputString);
  OutputDebugStringAReal(lpOutputString);
}

VOID WINAPI OutputDebugStringWWrap(_In_opt_ LPCWSTR lpOutputString)
{
  // intel gpu drivers spam this for some reason
  if (lpOutputString && wcsncmp(lpOutputString, L"Peiran message result", 21) == 0)
    return;

  Log("OutputDebugStringW: %S", lpOutputString);
  OutputDebugStringWReal(lpOutputString);
}


long count = 0;
int WINAPI FakeShowCursor(BOOL bShow)
{
  if (bShow)
    return InterlockedIncrement(&count);
  else
    return InterlockedDecrement(&count);
}


void setupConsole()
{
  if (AttachConsole(ATTACH_PARENT_PROCESS))
  {
    (void)freopen("CONIN$", "rb", stdin);
    (void)freopen("CONOUT$", "wb", stdout);
    (void)freopen("CONOUT$", "wb", stderr);
  }
}

// Optional ReShade support. With a DirectX wrapper such as dgVoodoo2 the usual ReShade proxy DLLs
// (dxgi.dll / d3d11.dll in the game folder) are not picked up, because the wrapper talks to the system
// Direct3D 11 directly. Loading ReShade32.dll into the process ourselves - the same thing ReShade's own
// injector does - lets it install its hooks before the wrapper creates its Direct3D 11 device.
// Only happens if the user put ReShade32.dll next to Game.exe; otherwise nothing changes.
static void loadReShade()
{
  if (GetFileAttributesA("ReShade32.dll") == INVALID_FILE_ATTRIBUTES)
    return;

  char fullPath[MAX_PATH] = {};
  GetFullPathNameA("ReShade32.dll", MAX_PATH, fullPath, nullptr);
  HMODULE reshade = LoadLibraryA(fullPath);
  Log("ReShade: loading %s -> %s (error %lu)\n", fullPath, reshade ? "OK" : "FAILED",
    reshade ? 0ul : (unsigned long)GetLastError());
}

void runHooks()
{
  Settings settings;
  settings.readSettings();

  if (settings.pauseOnStartup)
    MessageBoxA(nullptr, "Press OK to continue", "EmperorHooks", MB_OK);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

  setupConsole();

  loadReShade();

  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());
  DetourAttach(&(PVOID&)OutputDebugStringAReal, OutputDebugStringAWrap);
  DetourAttach(&(PVOID&)OutputDebugStringWReal, OutputDebugStringWWrap);

  if (settings.forceCursorVisible)
    DetourAttach(&(PVOID&)TrueShowCursor, FakeShowCursor);

  DetourTransactionCommit();

  patchDebugLog();
  HookD3D7(settings.widescreen, settings.pillarbox, settings.cutscene43, settings.upscaleToDesktop, settings.screenWidth, settings.screenHeight);
  patchCdCheck();
  patchRedirectRegistry();
  patchD3D7ResolutionLimit();
  patchWindowManagement(settings.fullscreen, !settings.disableCursorCapture, settings.upscaleToDesktop, settings.screenWidth, settings.screenHeight);

  // Cutscene (Bink) 4:3 pillarbox. Diagnostic-only for now: identifies the movie blit and confirms the
  // Bink path in emperor.txt while a cutscene plays. Safe to call regardless of the setting.
  initMovieFix(settings.cutscene43);

  // Framerate-normalized smooth map scrolling. Independent of widescreen, so it is installed here on its
  // own setting. Patches Game.exe once; the per-frame multiplier is driven from EndScene.
  initSmoothScroll(settings.smoothScroll);

  // In-game 2D HUD/sidebar widescreen fix, applied to the loaded image. Capture the vanilla bytes now;
  // it is toggled on ONLY while in a mission (see PickingTracer) so the front-end menu stays
  // vanilla/correct. The Hor+ camera dist-scale also starts here.
  if (settings.widescreen)
  {
    initHudFix();
    startPickingTracer();
  }

  //wrapWinsockWithLogging();

  if (!settings.hostGame)
    patchWolAsClient(settings.serverAddress);
  else
    patchWolAsServer();
}


// defer all our custom setup to just before WinMain rather than directly in DllMain, because there are a
// lot of restrictions on what you're allowed to do inside DllMain
PFN_WinMain WinMainOrig = GameExeWinMain;
int __stdcall WinMainPatched(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  runHooks();
  return WinMainOrig(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
}


__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (dwReason == DLL_PROCESS_ATTACH)
  {
    DetourRestoreAfterWith();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)WinMainOrig, WinMainPatched);
    DetourTransactionCommit();
  }

  return TRUE;
}