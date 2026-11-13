#pragma once
#include <windows.h>

// Registers EmperorHooks as a ReShade add-on (needs ReShade32.dll already loaded).
// Turns depth-based effects (ambient occlusion etc.) off in the front-end menus, where the 3D menu
// scene has no usable depth and those effects black out the screen, and back on in a mission.
void initReShadeAddon(HMODULE reshadeModule);
