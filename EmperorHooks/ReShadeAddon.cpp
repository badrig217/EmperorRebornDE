#include <windows.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include "reshade/reshade.hpp"
#include "ReShadeAddon.hpp"
#include "Log.hpp"

// EmperorHooks as a ReShade add-on (official add-on API, works with the normal ReShade build).
//
// Why: the "HD" preset uses Glamarye (ambient occlusion + fake global illumination), which needs the
// depth buffer. In a mission that works well. The front-end menus are a 3D scene too, but with depth
// values Glamarye cannot use, and it turns the whole menu black. Glamarye's own menu/sky detection does
// not catch this. So we switch such techniques off while the game is in a menu and back on in a mission.
//
// The game state comes from the mission flag Game.exe sets while a battlefield is running (the same
// flag PickingTracer uses for the in-game HUD fix). It is polled once per frame right before ReShade
// renders its effects, so the first menu frame is already rendered without the depth effects.
//
// Only the in-memory state of the techniques changes. ReShade saves the preset only when the user
// changes something in its menu. If that happens in a game menu, the depth effects are missing from the
// saved preset; we still remember that we switched them off and turn them on again in the mission.

using namespace reshade::api;

extern "C" __declspec(dllexport) const char* NAME = "Emperor Spice";
extern "C" __declspec(dllexport) const char* DESCRIPTION =
  "Turns depth-based effects (Glamarye) off in the game menus and on in missions.";

namespace
{
  const DWORD kMissionFlagVa = 0x00817c0c; // != 0 while a battlefield is running (see PickingTracer.cpp)

  // Techniques that need real scene depth. Matched by name prefix.
  const char* const kDepthTechniquePrefixes[] = { "Glamarye" };

  int g_state = -1;                       // last applied state: -1 = none yet, 0 = menu, 1 = mission
  std::vector<std::string> g_switchedOff; // depth techniques we turned off for the menu
  bool g_ourChange = false;               // true while we change technique states ourselves

  bool isDepthTechnique(const std::string& name)
  {
    for (const char* prefix : kDepthTechniquePrefixes)
      if (name.compare(0, strlen(prefix), prefix) == 0)
        return true;
    return false;
  }

  bool inMission()
  {
    return *(volatile DWORD*)kMissionFlagVa != 0;
  }

  std::string techniqueName(effect_runtime* runtime, effect_technique technique)
  {
    char name[256] = {};
    runtime->get_technique_name(technique, name);
    return name;
  }

  bool wasSwitchedOff(const std::string& name)
  {
    return std::find(g_switchedOff.begin(), g_switchedOff.end(), name) != g_switchedOff.end();
  }

  void applyState(effect_runtime* runtime, int state)
  {
    std::vector<effect_technique> techniques;
    runtime->enumerate_techniques(nullptr, [&techniques](effect_runtime*, effect_technique technique) {
      techniques.push_back(technique);
    });

    g_ourChange = true;
    for (effect_technique technique : techniques)
    {
      const std::string name = techniqueName(runtime, technique);
      if (!isDepthTechnique(name))
        continue;

      if (state == 0 && runtime->get_technique_state(technique))
      {
        runtime->set_technique_state(technique, false);
        if (!wasSwitchedOff(name))
          g_switchedOff.push_back(name);
        Log("ReShade add-on: %s OFF (menu)\n", name.c_str());
      }
      else if (state == 1 && wasSwitchedOff(name))
      {
        runtime->set_technique_state(technique, true);
        Log("ReShade add-on: %s ON (mission)\n", name.c_str());
      }
    }
    g_ourChange = false;

    if (state == 1)
      g_switchedOff.clear();
    g_state = state;
  }

  void onBeginEffects(effect_runtime* runtime, command_list*, resource_view, resource_view)
  {
    const int wanted = inMission() ? 1 : 0;
    if (wanted != g_state)
      applyState(runtime, wanted);
  }

  void onReloadedEffects(effect_runtime*)
  {
    // Technique handles are new and the preset was read again: apply the state again next frame.
    g_state = -1;
  }

  bool onSetTechniqueState(effect_runtime* runtime, effect_technique technique, bool enabled)
  {
    if (g_ourChange)
      return false;

    // The user switched a depth technique in the ReShade menu: that choice wins, we no longer
    // turn it back on by ourselves.
    const std::string name = techniqueName(runtime, technique);
    if (isDepthTechnique(name))
    {
      g_switchedOff.erase(std::remove(g_switchedOff.begin(), g_switchedOff.end(), name), g_switchedOff.end());
      Log("ReShade add-on: %s switched %s by the user\n", name.c_str(), enabled ? "on" : "off");
    }
    return false; // never block the change
  }
}

void initReShadeAddon(HMODULE reshadeModule)
{
  HMODULE self = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    reinterpret_cast<LPCSTR>(&initReShadeAddon), &self);

  if (self == nullptr || !reshade::register_addon(self, reshadeModule))
  {
    Log("ReShade add-on: registration FAILED (add-on API version %d, see ReShade.log)\n",
      RESHADE_API_VERSION);
    return;
  }

  reshade::register_event<reshade::addon_event::reshade_begin_effects>(&onBeginEffects);
  reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(&onReloadedEffects);
  reshade::register_event<reshade::addon_event::reshade_set_technique_state>(&onSetTechniqueState);
  Log("ReShade add-on: registered (depth effects off in menus, on in missions)\n");
}
