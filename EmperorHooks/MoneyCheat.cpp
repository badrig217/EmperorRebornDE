#include "MoneyCheat.hpp"
#include "Log.hpp"
#include <windows.h>

// Game.exe v1.09 (MD5 7dc4fda2f6b7e8a6b70e846686a81938) - found via the mission script interpreter
// (switch at 0x4ef5d5, handlers for GetPlayerSide / GetSideCash / AddSideCash / Multiplayer):
//   [0x7d75d4]            game setup object
//   0x5344e0  (thiscall)  setup->GetGameType()      3 = multiplayer (script command "Multiplayer")
//   0x5356a0  (thiscall)  setup->GetPlayerSide()    human player's side index outside multiplayer (read-only)
//   [0x817c00]            sides manager, non-null only while a mission is running
//   0x597bb0  (thiscall)  sides->GetSide(index)     side object or null
//   0x53ebf0  (thiscall)  side->AddCash(amount)     side cash lives at side+0x868 (same code as AddSideCash)
namespace
{
  bool gEnabled = false;
  bool gWasDown = false;
  const int kAmount = 25000;

  typedef int(__thiscall* IntGetterFn)(void* self);
  typedef void*(__thiscall* GetSideFn)(void* self, int index);
  typedef void(__thiscall* AddCashFn)(void* side, int amount);

  bool gameWindowFocused()
  {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg)
      GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
  }

  bool comboDown()
  {
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000) && (GetAsyncKeyState('G') & 0x8000);
  }

  void giveMoney()
  {
    void* setup = *reinterpret_cast<void**>(0x7d75d4);
    void* sides = *reinterpret_cast<void**>(0x817c00);
    if (!setup || !sides)
      return; // not in a mission

    int gameType = reinterpret_cast<IntGetterFn>(0x5344e0)(setup);
    if (gameType == 3)
    {
      Log("MoneyCheat: ignored - multiplayer game\n");
      return;
    }

    int playerSide = reinterpret_cast<IntGetterFn>(0x5356a0)(setup);
    void* side = reinterpret_cast<GetSideFn>(0x597bb0)(sides, playerSide);
    if (!side)
      return;

    reinterpret_cast<AddCashFn>(0x53ebf0)(side, kAmount);
    Log("MoneyCheat: +%d credits for side %d (now %d)\n", kAmount, playerSide, *reinterpret_cast<int*>(static_cast<char*>(side) + 0x868));
  }
}

void initMoneyCheat(bool enabled)
{
  gEnabled = enabled;
  Log("MoneyCheat: %s\n", enabled ? "enabled (Ctrl+Shift+G = +25000 credits, single player only)" : "disabled");
}

void moneyCheatUpdateFrame()
{
  if (!gEnabled)
    return;
  bool down = comboDown() && gameWindowFocused();
  if (down && !gWasDown) // once per key press, not every frame while held
    giveMoney();
  gWasDown = down;
}
