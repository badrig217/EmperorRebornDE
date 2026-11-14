#pragma once
// Emperor Spice graphics pack: dgVoodoo2 + ReShade, shipped in the "graphics" folder next to the launcher
// (downloaded at build time by fetch-graphics.ps1). When enabled, the launcher copies the files into the
// game folder before starting the game; when disabled, it removes them again - but only files that are
// identical to our bundled copies, so a player's own dgVoodoo/ReShade setup is never deleted.
//
// Two looks: "Modern" (ReShade preset Emperor_Modern.ini) and "HD" (Emperor_HD.ini plus 2x supersampling
// in dgVoodoo). The launcher switches between them by changing two settings in the game folder's copies
// of dgVoodoo.conf and ReShade.ini; everything else in those files stays as the player left it.
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace graphicsPack
{
  namespace fs = std::filesystem;

  // Their presence in the game folder is what switches dgVoodoo / ReShade on.
  inline const char* const activeFiles[] = { "DDraw.dll", "D3DImm.dll", "ReShade32.dll" };
  // Settings: copied only when missing, so tweaks the player makes (e.g. in the ReShade menu) survive.
  inline const char* const configFiles[] = { "dgVoodoo.conf", "ReShade.ini", "Emperor_Modern.ini", "Emperor_HD.ini" };

  inline bool equalsNoCase(const std::string& a, const std::string& b)
  {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
      return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
  }

  inline std::string trim(const std::string& text)
  {
    const size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos)
      return "";
    return text.substr(first, text.find_last_not_of(" \t") + 1 - first);
  }

  // Sets "key = value" inside [section] of an ini-style text file. Keeps everything else unchanged
  // (comments, alignment, line endings). If the key is missing it is added at the end of the section,
  // if the section is missing it is added at the end of the file. The file is only written if
  // something actually changes. Returns false if the file cannot be read or written.
  inline bool setIniValue(const fs::path& file, const std::string& section, const std::string& key, const std::string& value)
  {
    std::string content;
    {
      std::ifstream in(file, std::ios::binary);
      if (!in)
        return false;
      std::ostringstream buffer;
      buffer << in.rdbuf();
      content = buffer.str();
    }
    const std::string newline = content.find("\r\n") != std::string::npos ? "\r\n" : "\n";

    // Split into lines without their line endings.
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < content.size())
    {
      size_t end = content.find('\n', start);
      if (end == std::string::npos)
        end = content.size();
      std::string line = content.substr(start, end - start);
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      lines.push_back(line);
      start = end + 1;
    }

    bool inSection = false, sectionFound = false, done = false;
    size_t insertAt = lines.size(); // after the last non-empty line of the section
    for (size_t i = 0; i < lines.size() && !done; i++)
    {
      const std::string t = trim(lines[i]);
      if (!t.empty() && t.front() == '[')
      {
        const size_t close = t.find(']');
        inSection = close != std::string::npos && equalsNoCase(trim(t.substr(1, close - 1)), section);
        if (inSection)
        {
          sectionFound = true;
          insertAt = i + 1;
        }
        continue;
      }
      if (!inSection)
        continue;
      if (!t.empty())
        insertAt = i + 1;
      const size_t eq = lines[i].find('=');
      if (t.empty() || t.front() == ';' || t.front() == '#' || eq == std::string::npos)
        continue;
      if (!equalsNoCase(trim(lines[i].substr(0, eq)), key))
        continue;
      size_t valueStart = eq + 1;
      while (valueStart < lines[i].size() && (lines[i][valueStart] == ' ' || lines[i][valueStart] == '\t'))
        valueStart++;
      lines[i] = lines[i].substr(0, valueStart) + value;
      done = true;
    }

    if (!done)
    {
      if (!sectionFound)
      {
        if (!lines.empty() && !trim(lines.back()).empty())
          lines.push_back("");
        lines.push_back("[" + section + "]");
        insertAt = lines.size();
      }
      lines.insert(lines.begin() + insertAt, key + "=" + value);
    }

    std::string result;
    for (size_t i = 0; i < lines.size(); i++)
    {
      result += lines[i];
      if (i + 1 < lines.size() || (!content.empty() && content.back() == '\n'))
        result += newline;
    }
    if (result == content)
      return true;

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << result;
    return static_cast<bool>(out);
  }

  // Switches the game folder's dgVoodoo/ReShade settings to the "Modern" or "HD" look.
  inline std::wstring setStyle(const fs::path& gameDir, bool hd)
  {
    // [DirectX] Resolution: dgVoodoo renders at 2x the game resolution and scales down (supersampling,
    // smooth edges without MSAA, which hangs the game). Only the [DirectX] key - [Glide] has one too.
    if (!setIniValue(gameDir / "dgVoodoo.conf", "DirectX", "Resolution", hd ? "2x" : "unforced"))
      return L"dgVoodoo.conf";
    if (hd && !setIniValue(gameDir / "dgVoodoo.conf", "GeneralExt", "Resampling", "lanczos-3"))
      return L"dgVoodoo.conf";
    if (!setIniValue(gameDir / "ReShade.ini", "GENERAL", "PresetPath", hd ? ".\\Emperor_HD.ini" : ".\\Emperor_Modern.ini"))
      return L"ReShade.ini";
    return L"";
  }

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
  inline std::wstring apply(const fs::path& sourceDir, const fs::path& gameDir, bool enabled, bool hd)
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
          {
            const std::string msg = ec.message(); // one string: begin() and end() must come from the same object
            return L"copy " + fs::path(name).wstring() + L": " + std::wstring(msg.begin(), msg.end());
          }
        }
      }
      for (const char* name : configFiles)
      {
        if (!fs::exists(gameDir / name, ec))
          fs::copy_file(sourceDir / name, gameDir / name, ec);
      }
      fs::copy(sourceDir / "reshade-shaders", gameDir / "reshade-shaders",
               fs::copy_options::recursive | fs::copy_options::skip_existing, ec);

      std::wstring styleError = setStyle(gameDir, hd);
      if (!styleError.empty())
        return L"update " + styleError;
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
