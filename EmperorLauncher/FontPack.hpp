#pragma once
// Emperor Spice large in-game text: two extra game archives with 2x fonts (built by
// tools/fonts/build_fontpack.py), shipped in the "fonts" folder next to the launcher.
//
// The game loads its archives as <prefix>0001, <prefix>0002, ... and stops at the first missing
// number, so ours have to take the next free number. The fonts inside have new names (SmallHD,
// StandardHD) and change nothing on their own - EmperorHooks only switches to them when the setting
// is on. That is why the archives are never removed again: leaving them in place is harmless and
// avoids renumbering other archives.
#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include "GraphicsPack.hpp"

namespace fontPack
{
  namespace fs = std::filesystem;

  inline const wchar_t* const prefixes[] = { L"UI", L"3DDATA" };

  inline bool available(const fs::path& sourceDir)
  {
    std::error_code ec;
    for (const wchar_t* p : prefixes)
      if (!fs::is_regular_file(sourceDir / (std::wstring(p) + L".RFH"), ec) || !fs::is_regular_file(sourceDir / (std::wstring(p) + L".RFD"), ec))
        return false;
    return true;
  }

  inline std::wstring archiveName(const wchar_t* prefix, int number, const wchar_t* ext)
  {
    wchar_t name[64];
    swprintf(name, 64, L"%ls%04d%ls", prefix, number, ext);
    return name;
  }

  // Returns an empty string on success, otherwise a description of what failed.
  inline std::wstring install(const fs::path& sourceDir, const fs::path& dataDir)
  {
    std::error_code ec;
    if (!available(sourceDir))
      return L""; // nothing bundled (developer build)
    if (!fs::is_directory(dataDir, ec))
      return L"DATA folder not found: " + dataDir.wstring();

    for (const wchar_t* prefix : prefixes)
    {
      const fs::path ourRfh = sourceDir / (std::wstring(prefix) + L".RFH");
      const fs::path ourRfd = sourceDir / (std::wstring(prefix) + L".RFD");

      int number = 1;
      bool installed = false;
      for (; number < 100; number++)
      {
        fs::path rfh = dataDir / archiveName(prefix, number, L".RFH");
        fs::path rfd = dataDir / archiveName(prefix, number, L".RFD");
        if (!fs::exists(rfh, ec) || !fs::exists(rfd, ec))
          break; // first free number - the game stops loading here
        if (graphicsPack::sameContent(ourRfh, rfh) && graphicsPack::sameContent(ourRfd, rfd))
        {
          installed = true;
          break;
        }
      }
      if (installed)
        continue;
      if (number >= 100)
        return std::wstring(prefix) + L": no free archive number";

      fs::path rfh = dataDir / archiveName(prefix, number, L".RFH");
      fs::path rfd = dataDir / archiveName(prefix, number, L".RFD");
      // Data first, index last: a half-copied pair without its index is never picked up by the game.
      fs::remove(rfh, ec);
      fs::copy_file(ourRfd, rfd, fs::copy_options::overwrite_existing, ec);
      if (!ec)
        fs::copy_file(ourRfh, rfh, fs::copy_options::overwrite_existing, ec);
      if (ec)
      {
        std::string msg = ec.message();
        fs::remove(rfh, ec);
        fs::remove(rfd, ec);
        return L"copy " + rfh.filename().wstring() + L": " + std::wstring(msg.begin(), msg.end());
      }
    }
    return L"";
  }
}
