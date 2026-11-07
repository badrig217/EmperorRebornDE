#include <windows.h>
#include <cstring>
#include "CameraZoom.hpp"
#include "Log.hpp"

// Game.exe 1.09 reads UI\CAMERA.INI in 0x532xxx. For "MinZoom" (the farthest zoom, 0.5 in the
// original file) it does
//
//   0x532e59  call  atof
//   0x532e5e  fdivr dword ptr [0x5d0720]     ; 1.0f / MinZoom
//   0x532e67  fstp  dword ptr [edi + 0x30]   ; = largest camera distance factor (2.0 originally)
//
// We point that fdivr at our own constant instead of the shared 1.0f, so the largest distance factor
// becomes k / MinZoom. Everything else about the camera (closest zoom, tilt, start fly-in) is unchanged.
namespace
{
  const DWORD kFdivrVa = 0x532e5e;
  const unsigned char kFdivrOrig[6] = { 0xd8, 0x3d, 0x20, 0x07, 0x5d, 0x00 }; // fdivr [0x5d0720]
  const DWORD kOneVa = 0x5d0720;

  float gZoomOutFactor = 1.0f; // must stay alive: the game reads it whenever CAMERA.INI is parsed
}

void initCameraZoom(int percent)
{
  if (percent <= 100)
  {
    Log("CameraZoom: original\n");
    return;
  }
  if (percent > 300)
    percent = 300;

  if (memcmp(reinterpret_cast<const void*>(kFdivrVa), kFdivrOrig, sizeof(kFdivrOrig)) != 0 ||
      *reinterpret_cast<const float*>(kOneVa) != 1.0f)
  {
    Log("CameraZoom: unexpected code at %08lX, not patching\n", (unsigned long)kFdivrVa);
    return;
  }

  gZoomOutFactor = percent / 100.0f;
  DWORD operand = reinterpret_cast<DWORD>(&gZoomOutFactor);

  DWORD oldProt = 0;
  if (!VirtualProtect(reinterpret_cast<void*>(kFdivrVa + 2), 4, PAGE_EXECUTE_READWRITE, &oldProt))
  {
    Log("CameraZoom: VirtualProtect failed (err %lu)\n", GetLastError());
    return;
  }
  memcpy(reinterpret_cast<void*>(kFdivrVa + 2), &operand, 4);
  VirtualProtect(reinterpret_cast<void*>(kFdivrVa + 2), 4, oldProt, &oldProt);
  FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(kFdivrVa), 6);
  Log("CameraZoom: farthest zoom %d%% of the original\n", percent);
}
