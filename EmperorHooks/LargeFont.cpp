#include "LargeFont.hpp"
#include "Log.hpp"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Game.exe v1.09 (MD5 7dc4fda2f6b7e8a6b70e846686a81938), CFont3D:
//   0x5d3540  font table in .rdata, 16-byte records, terminated by a null name:
//             +0 const char* name ("Small", "Standard", ...), +4 u8 row height in pixels,
//             +5 u8 flag, +8 i32 page count, +0xc u8 flag
//   0x4ab1a0  loads one font: "FONT\<name><page>.MAP" (UTF-16 character list, from the UI archives)
//             and "FONT\<name><page>.TGA" (glyph sheet; the texture manager looks it up by file name
//             in the 3DDATA archives). Pages > 1 use the "<name><page>" form, single-page fonts "<name>".
//   0x4ab330  glyph scanner: walks the sheet in rows of <row height>, a glyph is a run of non-black
//             columns. UVs are derived from the sheet size, so a 2x sheet with 2x row height simply
//             draws 2x larger, sharp text.
namespace
{
  struct FontRecord
  {
    const char* name;
    unsigned char rowHeight;
    unsigned char flag5;
    unsigned char pad[2];
    int pages;
    unsigned char flagC;
    unsigned char pad2[3];
  };
  static_assert(sizeof(FontRecord) == 16, "font table record size");

  FontRecord* const kFontTable = reinterpret_cast<FontRecord*>(0x5d3540);

  const char kSmallHd[] = "SmallHD";

  std::string dataDir()
  {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string dir(path);
    dir.resize(dir.find_last_of("\\/") + 1);
    return dir + "DATA\\";
  }

  // The game loads <prefix>0001, 0002, ... until the first missing number; look through exactly those.
  bool archiveSetContains(const std::string& dir, const char* prefix, const char* entry)
  {
    for (int i = 1; i < 100; i++)
    {
      char name[64];
      snprintf(name, sizeof(name), "%s%04d.RFH", prefix, i);
      FILE* f = fopen((dir + name).c_str(), "rb");
      if (!f)
        return false;
      std::vector<char> data;
      char buf[4096];
      size_t n;
      while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        data.insert(data.end(), buf, buf + n);
      fclose(f);
      size_t len = strlen(entry);
      for (size_t p = 0; p + len <= data.size(); p++)
        if (_strnicmp(&data[p], entry, len) == 0)
          return true;
    }
    return false;
  }

  bool sameName(const char* a, const char* b) { return a && _stricmp(a, b) == 0; }
}

void initLargeFont(bool enabled)
{
  if (!enabled)
  {
    Log("LargeFont: disabled\n");
    return;
  }

  std::string dir = dataDir();
  if (!archiveSetContains(dir, "UI", "FONT\\SMALLHD0.MAP") || !archiveSetContains(dir, "UI", "FONT\\STANDARDHD.MAP") ||
      !archiveSetContains(dir, "3DDATA", "Textures\\SmallHD0.tga") || !archiveSetContains(dir, "3DDATA", "Textures\\StandardHD.tga"))
  {
    Log("LargeFont: HD font archives not found in %s - keeping the original fonts\n", dir.c_str());
    return;
  }

  DWORD oldProtect = 0;
  if (!VirtualProtect(kFontTable, sizeof(FontRecord) * 8, PAGE_READWRITE, &oldProtect))
  {
    Log("LargeFont: VirtualProtect failed (%lu)\n", GetLastError());
    return;
  }

  int patched = 0;
  for (FontRecord* r = kFontTable; r->name && r < kFontTable + 8; r++)
  {
    if (sameName(r->name, "Small") && r->rowHeight == 16 && r->pages == 2)
    {
      r->name = kSmallHd;
      r->rowHeight = 32;
      patched++;
    }
    // "Standard" (menus, options, buttons, loading screen) stays as it is: the game already sizes those
    // texts with the resolution, so a 2x sheet makes them twice too big (tested at 720p and 1440p).
    // Only "Small" (in-game messages) is drawn in fixed pixels and needs the HD sheet.
  }

  VirtualProtect(kFontTable, sizeof(FontRecord) * 8, oldProtect, &oldProtect);
  Log("LargeFont: %d font(s) switched to HD (2x)\n", patched);
}
