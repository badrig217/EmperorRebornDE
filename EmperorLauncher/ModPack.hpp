#pragma once
// Emperor Spice mod folder: loose files in  mods\<ModName>\<ARCHIVE>\<path inside the archive>
// are packed into extra game archives at every launch, e.g.
//
//   mods\CheaperWalls\MODEL\Rules.txt          -> replaces Rules.txt from the MODEL archives
//   mods\MyIcons\3DDATA\Textures\AT_Wall.tga   -> replaces that texture
//   mods\FarCamera\UI\CAMERA.INI               -> replaces the camera settings
//
// How the game picks this up (see docs/reverse-engineering.md):
//  - it loads <ARCHIVE>0001, 0002, ... and stops at the first missing number, so our archive takes
//    the next free number;
//  - if two archives contain the same entry, the one with the newer timestamp wins, so our entries
//    get the current time.
// Mods are applied in alphabetical order of their folder names; for the same file the later mod wins.
// A mod folder whose name starts with '_' is ignored (quick way to switch a mod off).
//
// Our archives contain the marker entry "EmperorSpice\ModPack.txt". At every launch the previous ones
// are removed first and the archives above them are renumbered, so no gap is left behind.
// Entries are stored uncompressed (flag 0), which the original archives use as well.
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace modPack
{
  namespace fs = std::filesystem;

  inline const char kMarkerName[] = "EmperorSpice\\ModPack.txt";

  struct Result
  {
    int modCount = 0;      // mod folders that were applied
    int fileCount = 0;     // files packed in total
    std::wstring error;    // empty on success
    std::wstring warnings; // e.g. unknown archive folder names
  };

  inline std::wstring lower(std::wstring s)
  {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
    return s;
  }

  inline std::wstring archiveName(const std::wstring& prefix, int number, const wchar_t* ext)
  {
    wchar_t name[96];
    swprintf(name, 96, L"%ls%04d%ls", prefix.c_str(), number, ext);
    return name;
  }

  inline bool readFile(const fs::path& path, std::vector<char>& out)
  {
    std::ifstream f(path, std::ios::binary);
    if (!f)
      return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
  }

  // Entry names in an .RFH index (u32 nameLen, ts, flags, stored, raw, offset, char name[nameLen]).
  inline std::vector<std::string> readIndexNames(const fs::path& rfh)
  {
    std::vector<std::string> names;
    std::vector<char> d;
    if (!readFile(rfh, d))
      return names;
    size_t pos = 0;
    while (pos + 24 <= d.size())
    {
      uint32_t nameLen = 0;
      memcpy(&nameLen, &d[pos], 4);
      pos += 24;
      if (nameLen == 0 || pos + nameLen > d.size())
        break;
      names.emplace_back(&d[pos], strnlen(&d[pos], nameLen));
      pos += nameLen;
    }
    return names;
  }

  inline bool isOurArchive(const fs::path& rfh)
  {
    for (const std::string& n : readIndexNames(rfh))
      if (_stricmp(n.c_str(), kMarkerName) == 0)
        return true;
    return false;
  }

  // Archive prefixes the game uses, taken from the <PREFIX>0001.RFH files in DATA (e.g. "3DDATA", "UI").
  inline std::vector<std::wstring> gamePrefixes(const fs::path& dataDir)
  {
    std::vector<std::wstring> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dataDir, ec))
    {
      std::wstring n = e.path().filename().wstring();
      std::wstring ln = lower(n);
      if (ln.size() > 8 && ln.compare(ln.size() - 8, 8, L"0001.rfh") == 0)
        out.push_back(n.substr(0, n.size() - 8));
    }
    return out;
  }

  // Remove our previous archives for one prefix and close the gaps they leave.
  inline std::wstring removeOldAndCompact(const fs::path& dataDir, const std::wstring& prefix)
  {
    std::error_code ec;
    std::vector<int> kept;
    for (int n = 1; n < 100; n++)
    {
      fs::path rfh = dataDir / archiveName(prefix, n, L".RFH");
      fs::path rfd = dataDir / archiveName(prefix, n, L".RFD");
      bool hasRfh = fs::exists(rfh, ec);
      if (!hasRfh)
        break; // the game stops loading at the first missing index; never pull in archives beyond it
      if (hasRfh && isOurArchive(rfh))
      {
        // Index first: without its index a data file is never used by the game.
        fs::remove(rfh, ec);
        fs::remove(rfd, ec);
        continue;
      }
      kept.push_back(n);
    }

    // Renumber the remaining archives 1, 2, 3, ... in their original order. Archives from the CDs and
    // the patch are at the bottom without gaps, so they keep their numbers; only archives that sat above
    // a removed one (e.g. the font pack) move down.
    int target = 1;
    for (int n : kept)
    {
      if (n != target)
      {
        for (const wchar_t* ext : { L".RFD", L".RFH" })
        {
          fs::path from = dataDir / archiveName(prefix, n, ext);
          if (fs::exists(from, ec))
            fs::rename(from, dataDir / archiveName(prefix, target, ext), ec);
          if (ec)
            return L"rename " + from.filename().wstring() + L" failed";
        }
      }
      target++;
    }
    return L"";
  }

  inline int nextFreeNumber(const fs::path& dataDir, const std::wstring& prefix)
  {
    std::error_code ec;
    int n = 1;
    while (n < 100 && fs::exists(dataDir / archiveName(prefix, n, L".RFH"), ec))
      n++;
    return n;
  }

  struct Entry
  {
    std::string name;   // as stored in the archive, e.g. "Textures\AT_Wall.tga"
    fs::path source;
    uint32_t timestamp;
  };

  inline std::string toArchiveName(const fs::path& rel)
  {
    std::string out;
    for (const auto& part : rel)
    {
      if (!out.empty())
        out += '\\';
      std::wstring w = part.wstring();
      // Archive names are 8-bit (Latin-1 / Windows-1252) like the original data.
      char buf[512];
      int len = WideCharToMultiByte(1252, 0, w.c_str(), int(w.size()), buf, sizeof(buf), nullptr, nullptr);
      out.append(buf, len > 0 ? len : 0);
    }
    return out;
  }

  inline std::wstring writeArchive(const fs::path& dataDir, const std::wstring& prefix, int number,
                                   const std::vector<Entry>& entries, const std::string& markerText)
  {
    std::vector<char> rfh, rfd;
    auto put32 = [](std::vector<char>& v, uint32_t x) { v.insert(v.end(), reinterpret_cast<char*>(&x), reinterpret_cast<char*>(&x) + 4); };
    auto add = [&](const std::string& name, uint32_t ts, const std::vector<char>& content)
    {
      put32(rfh, uint32_t(name.size() + 1));
      put32(rfh, ts);
      put32(rfh, 0);                           // flags: stored uncompressed
      put32(rfh, uint32_t(content.size()));    // stored size
      put32(rfh, uint32_t(content.size()));    // raw size
      put32(rfh, uint32_t(rfd.size()));        // offset
      rfh.insert(rfh.end(), name.begin(), name.end());
      rfh.push_back('\0');
      rfd.insert(rfd.end(), content.begin(), content.end());
    };

    std::vector<char> content;
    for (const Entry& e : entries)
    {
      if (!readFile(e.source, content))
        return L"cannot read " + e.source.wstring();
      add(e.name, e.timestamp, content);
    }
    add(kMarkerName, uint32_t(time(nullptr)), std::vector<char>(markerText.begin(), markerText.end()));

    fs::path rfhPath = dataDir / archiveName(prefix, number, L".RFH");
    fs::path rfdPath = dataDir / archiveName(prefix, number, L".RFD");
    // Data first, index last: a data file without its index is never picked up by the game.
    {
      std::ofstream f(rfdPath, std::ios::binary | std::ios::trunc);
      f.write(rfd.data(), rfd.size());
      if (!f)
        return L"cannot write " + rfdPath.wstring();
    }
    {
      std::ofstream f(rfhPath, std::ios::binary | std::ios::trunc);
      f.write(rfh.data(), rfh.size());
      if (!f)
      {
        std::error_code ec;
        fs::remove(rfdPath, ec);
        return L"cannot write " + rfhPath.wstring();
      }
    }
    return L"";
  }

  // Rebuild the mod archives in dataDir from modsDir. With enabled == false (or no mods) this only
  // removes the archives of an earlier launch, so the game runs unmodified.
  inline Result apply(const fs::path& modsDir, const fs::path& dataDir, bool enabled)
  {
    Result result;
    std::error_code ec;
    if (!fs::is_directory(dataDir, ec))
    {
      result.error = L"DATA folder not found: " + dataDir.wstring();
      return result;
    }

    std::vector<std::wstring> prefixes = gamePrefixes(dataDir);
    for (const std::wstring& prefix : prefixes)
    {
      result.error = removeOldAndCompact(dataDir, prefix);
      if (!result.error.empty())
        return result;
    }

    if (!enabled || !fs::is_directory(modsDir, ec))
      return result;

    std::vector<fs::path> mods;
    for (const auto& e : fs::directory_iterator(modsDir, ec))
      if (e.is_directory(ec) && e.path().filename().wstring().rfind(L"_", 0) != 0)
        mods.push_back(e.path());
    std::sort(mods.begin(), mods.end(), [](const fs::path& a, const fs::path& b) { return lower(a.filename().wstring()) < lower(b.filename().wstring()); });

    // prefix (lower case) -> entry name (lower case) -> entry. Later mods overwrite earlier ones.
    std::map<std::wstring, std::map<std::string, Entry>> byPrefix;
    const uint32_t now = uint32_t(time(nullptr));
    std::string markerText = "Emperor Spice mod pack. Rebuilt by the launcher at every start - do not edit.\r\nMods:\r\n";

    for (size_t i = 0; i < mods.size(); i++)
    {
      bool used = false;
      for (const auto& sub : fs::directory_iterator(mods[i], ec))
      {
        if (!sub.is_directory(ec))
          continue;
        std::wstring folder = sub.path().filename().wstring();
        auto prefixIt = std::find_if(prefixes.begin(), prefixes.end(), [&](const std::wstring& p) { return lower(p) == lower(folder); });
        if (prefixIt == prefixes.end())
        {
          result.warnings += mods[i].filename().wstring() + L"\\" + folder + L"\n";
          continue;
        }
        for (auto it = fs::recursive_directory_iterator(sub.path(), ec); it != fs::recursive_directory_iterator(); it.increment(ec))
        {
          if (ec || !it->is_regular_file(ec))
            continue;
          Entry entry;
          entry.name = toArchiveName(fs::relative(it->path(), sub.path(), ec));
          entry.source = it->path();
          entry.timestamp = now + uint32_t(i); // newer than the original, and later mods newer than earlier ones
          std::string key = entry.name;
          std::transform(key.begin(), key.end(), key.begin(), [](char c) { return (char)tolower((unsigned char)c); });
          byPrefix[lower(*prefixIt)][key] = entry;
          used = true;
        }
      }
      if (used)
      {
        result.modCount++;
        markerText += toArchiveName(mods[i].filename()) + "\r\n";
      }
    }

    for (const std::wstring& prefix : prefixes)
    {
      auto found = byPrefix.find(lower(prefix));
      if (found == byPrefix.end() || found->second.empty())
        continue;
      std::vector<Entry> entries;
      for (auto& kv : found->second)
        entries.push_back(kv.second);
      int number = nextFreeNumber(dataDir, prefix);
      if (number >= 100)
      {
        result.error = prefix + L": no free archive number";
        return result;
      }
      result.error = writeArchive(dataDir, prefix, number, entries, markerText);
      if (!result.error.empty())
        return result;
      result.fileCount += int(entries.size());
    }
    return result;
  }
}
