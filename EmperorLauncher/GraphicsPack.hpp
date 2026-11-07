#pragma once
// Emperor Spice graphics pack: dgVoodoo2 + ReShade, shipped in the "graphics" folder next to the launcher
// (downloaded at build time by fetch-graphics.ps1). When enabled, the launcher copies the files into the
// game folder before starting the game; when disabled, it removes them again - but only files that are
// identical to our bundled copies, so a player's own dgVoodoo/ReShade setup is never deleted.
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace graphicsPack
{
  namespace fs = std::filesystem;

  // Their presence in the game folder is what switches dgVoodoo / ReShade on.
  inline const char* const activeFiles[] = { "DDraw.dll", "D3DImm.dll", "ReShade32.dll" };
  // Settings: copied only when missing, so tweaks the player makes (e.g. in the ReShade menu) survive.
  inline const char* const configFiles[] = { "dgVoodoo.conf", "ReShade.ini", "Emperor_Modern.ini" };

  inline bool sameContent(const fs::path& a, const fs::path& b)
  {
    std::error_code ec;
    if (!fs::is_regular_file(a, ec) || !fs::is_regular_file(b, ec))
      return false;
    if (fs::file_size(a, ec) != fs::file_size(b, ec) || ec)
      return false;
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    return std::equal(std::istreambuf_iterator<char>(fa), std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(fb));
  }

  inline bool available(const fs::path& sourceDir)
  {
    std::error_code ec;
    return fs::is_regular_file(sourceDir / "DDraw.dll", ec) && fs::is_regular_file(sourceDir / "ReShade32.dll", ec);
  }

  // Returns an empty string on success, otherwise a description of what failed.
  inline std::wstring apply(const fs::path& sourceDir, const fs::path& gameDir, bool enabled)
  {
    std::error_code ec;
    if (!available(sourceDir))
      return L""; // nothing bundled (e.g. a developer build) - leave the game folder alone

    if (enabled)
    {
      for (const char* name : activeFiles)
      {
        if (!sameContent(sourceDir / name, gameDir / name))
        {
          fs::copy_file(sourceDir / name, gameDir / name, fs::copy_options::overwrite_existing, ec);
          if (ec)
            return L"copy " + fs::path(name).wstring() + L": " + std::wstring(ec.message().begin(), ec.message().end());
        }
      }
      for (const char* name : configFiles)
      {
        if (!fs::exists(gameDir / name, ec))
          fs::copy_file(sourceDir / name, gameDir / name, ec);
      }
      fs::copy(sourceDir / "reshade-shaders", gameDir / "reshade-shaders",
               fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
    }
    else
    {
      for (const char* name : activeFiles)
      {
        if (sameContent(sourceDir / name, gameDir / name))
          fs::remove(gameDir / name, ec);
      }
    }
    return L"";
  }
}
