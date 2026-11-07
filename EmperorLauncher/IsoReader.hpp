#pragma once
// Minimal ISO 9660 reader (with Joliet long names) - enough to install Emperor from CD images
// (.iso) without mounting them in Windows. Read-only, no external dependencies.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace iso
{
  const uint32_t kSector = 2048;

  struct Entry
  {
    std::wstring name;
    bool directory = false;
    std::vector<std::pair<uint32_t, uint32_t>> extents; // (first sector, size in bytes); several for multi-extent files
    uint64_t size() const { uint64_t s = 0; for (auto& e : extents) s += e.second; return s; }
  };

  class Image
  {
  public:
    explicit Image(const std::filesystem::path& path) { f_ = _wfopen(path.wstring().c_str(), L"rb"); if (f_) init(); }
    ~Image() { if (f_) fclose(f_); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    bool valid() const { return valid_; }
    const std::string& volumeLabel() const { return label_; }

    // Extracts the whole image into destDir. progress(relativePath) is called for every file.
    // Returns false on a read/write error.
    bool extractAll(const std::filesystem::path& destDir, const std::function<void(const std::wstring&)>& progress = {})
    {
      if (!valid_)
        return false;
      return extractDir(root_, destDir, L"", progress, 0);
    }

    // True if a file exists at relPath (backslash separated, case-insensitive), e.g. L"INSTALL\\Game1.cab".
    bool fileExists(const std::wstring& relPath)
    {
      if (!valid_)
        return false;
      Entry current = root_;
      size_t start = 0;
      while (true)
      {
        size_t sep = relPath.find(L'\\', start);
        std::wstring part = relPath.substr(start, sep == std::wstring::npos ? std::wstring::npos : sep - start);
        std::vector<Entry> entries;
        if (!listDir(current, entries))
          return false;
        bool found = false;
        for (auto& e : entries)
        {
          if (_wcsicmp(e.name.c_str(), part.c_str()) == 0)
          {
            if (sep == std::wstring::npos)
              return !e.directory;
            if (!e.directory)
              return false;
            current = e;
            found = true;
            break;
          }
        }
        if (!found)
          return false;
        start = sep + 1;
      }
    }

  private:
    FILE* f_ = nullptr;
    bool valid_ = false;
    bool joliet_ = false;
    std::string label_;
    Entry root_;

    bool readSectors(uint32_t sector, uint32_t bytes, std::vector<uint8_t>& out)
    {
      out.resize(bytes);
      if (_fseeki64(f_, int64_t(sector) * kSector, SEEK_SET) != 0)
        return false;
      return fread(out.data(), 1, bytes, f_) == bytes;
    }

    static uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }

    void init()
    {
      std::vector<uint8_t> vd;
      bool havePrimary = false;
      std::vector<uint8_t> primaryRoot, jolietRoot;
      for (uint32_t s = 16; s < 16 + 32; s++)
      {
        if (!readSectors(s, kSector, vd) || memcmp(&vd[1], "CD001", 5) != 0)
          return;
        uint8_t type = vd[0];
        if (type == 1 && !havePrimary)
        {
          havePrimary = true;
          label_.assign(reinterpret_cast<const char*>(&vd[40]), 32);
          while (!label_.empty() && (label_.back() == ' ' || label_.back() == '\0'))
            label_.pop_back();
          primaryRoot.assign(&vd[156], &vd[156] + 34);
        }
        else if (type == 2 && vd[88] == 0x25 && vd[89] == 0x2F && (vd[90] == 0x40 || vd[90] == 0x43 || vd[90] == 0x45))
        {
          jolietRoot.assign(&vd[156], &vd[156] + 34); // Joliet supplementary descriptor (UCS-2 names)
        }
        else if (type == 255)
          break;
      }
      if (!havePrimary)
        return;
      joliet_ = !jolietRoot.empty();
      const std::vector<uint8_t>& r = joliet_ ? jolietRoot : primaryRoot;
      root_.directory = true;
      root_.extents.push_back({ le32(&r[2]), le32(&r[10]) });
      valid_ = true;
    }

    std::wstring decodeName(const uint8_t* id, uint8_t len) const
    {
      std::wstring name;
      if (joliet_)
      {
        for (uint8_t i = 0; i + 1 < len; i += 2)
          name.push_back(wchar_t((id[i] << 8) | id[i + 1]));
      }
      else
      {
        for (uint8_t i = 0; i < len; i++)
          name.push_back(wchar_t(id[i]));
      }
      size_t semi = name.find(L';');            // strip version suffix ";1"
      if (semi != std::wstring::npos)
        name.resize(semi);
      if (!name.empty() && name.back() == L'.') // "NAME." -> "NAME"
        name.pop_back();
      return name;
    }

    bool listDir(const Entry& dir, std::vector<Entry>& out)
    {
      std::vector<uint8_t> data;
      if (!readSectors(dir.extents[0].first, dir.extents[0].second, data))
        return false;
      uint32_t pos = 0;
      while (pos < data.size())
      {
        uint8_t len = data[pos];
        if (len == 0) { pos = (pos / kSector + 1) * kSector; continue; } // padding up to next sector
        if (pos + len > data.size() || len < 34)
          return false;
        const uint8_t* rec = &data[pos];
        uint8_t flags = rec[25];
        uint8_t nameLen = rec[32];
        bool special = nameLen == 1 && (rec[33] == 0 || rec[33] == 1); // "." and ".."
        if (!special && !(flags & 0x01))                            // skip hidden/associated entries
        {
          std::wstring name = decodeName(&rec[33], nameLen);
          std::pair<uint32_t, uint32_t> extent{ le32(&rec[2]), le32(&rec[10]) };
          if (!out.empty() && out.back().name == name && !out.back().directory && !(flags & 0x02) && out.back().extents.size() && out.back().extents.back().second > 0 && multiExtentPending_)
            out.back().extents.push_back(extent);                    // continuation of a multi-extent file
          else
          {
            Entry e;
            e.name = name;
            e.directory = (flags & 0x02) != 0;
            e.extents.push_back(extent);
            out.push_back(e);
          }
          multiExtentPending_ = (flags & 0x80) != 0;
        }
        pos += len;
      }
      return true;
    }
    bool multiExtentPending_ = false;

    bool extractFile(const Entry& e, const std::filesystem::path& dest)
    {
      FILE* out = _wfopen(dest.wstring().c_str(), L"wb");
      if (!out)
        return false;
      std::vector<uint8_t> buf(1 << 20);
      bool ok = true;
      for (auto& ext : e.extents)
      {
        if (_fseeki64(f_, int64_t(ext.first) * kSector, SEEK_SET) != 0) { ok = false; break; }
        uint32_t left = ext.second;
        while (left > 0 && ok)
        {
          uint32_t n = left < buf.size() ? left : uint32_t(buf.size());
          ok = fread(buf.data(), 1, n, f_) == n && fwrite(buf.data(), 1, n, out) == n;
          left -= n;
        }
      }
      if (fclose(out) != 0)
        ok = false;
      return ok;
    }

    bool extractDir(const Entry& dir, const std::filesystem::path& dest, const std::wstring& rel,
                    const std::function<void(const std::wstring&)>& progress, int depth)
    {
      if (depth > 16)
        return false;
      std::error_code ec;
      std::filesystem::create_directories(dest, ec);
      std::vector<Entry> entries;
      if (!listDir(dir, entries))
        return false;
      for (auto& e : entries)
      {
        std::wstring childRel = rel.empty() ? e.name : rel + L"\\" + e.name;
        if (e.directory)
        {
          if (!extractDir(e, dest / e.name, childRel, progress, depth + 1))
            return false;
        }
        else
        {
          if (progress)
            progress(childRel);
          if (!extractFile(e, dest / e.name))
            return false;
        }
      }
      return true;
    }
  };

  // Which Emperor disc an image is: "EMPEROR1".."EMPEROR4" when both the volume label and the content
  // match (disc 1 has INSTALL\\Game1.cab, discs 2-4 have MOVIES0001.RFD), otherwise "".
  inline std::string emperorDisc(const std::filesystem::path& path)
  {
    Image img(path);
    if (!img.valid())
      return "";
    const std::string& label = img.volumeLabel();
    if (label == "EMPEROR1")
      return img.fileExists(L"INSTALL\\Game1.cab") ? label : "";
    if (label == "EMPEROR2" || label == "EMPEROR3" || label == "EMPEROR4")
      return img.fileExists(L"MOVIES0001.RFD") ? label : "";
    return "";
  }

  // Label (volume name) of an ISO image, or "" if the file is not a readable ISO 9660 image.
  inline std::string volumeLabel(const std::filesystem::path& path)
  {
    Image img(path);
    return img.valid() ? img.volumeLabel() : std::string();
  }
}
