#pragma once
// Platform-independent part of the HD texture workshop (see TextureWorkshop.hpp): reading the game
// archives, sorting the textures, the recipes before/after Real-ESRGAN, and writing/moving the result
// archive. Upscaling itself is done by a runner function supplied by the caller (Windows: the
// launcher starts realesrgan-ncnn-vulkan.exe; tests: the Linux build of the same program).
// stb: define STB_IMAGE_IMPLEMENTATION and STB_IMAGE_WRITE_IMPLEMENTATION in exactly one translation unit
// before including this file (TextureWorkshop.cpp does).
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "HdImage.hpp"
#include "third_party/stb_image.h"
#include "third_party/stb_image_write.h"

namespace hdCore
{
  namespace fs = std::filesystem;
  using hdImage::Img;

  inline const char kHdMarker[] = "EmperorSpice\\HDTextures.txt";
  inline const char kModMarker[] = "EmperorSpice\\ModPack.txt";
  inline const char kRecipeVersion[] = "HD textures v1 (icons 2x, units/buildings 2x + remaster, terrain 4x)";

  inline std::string lowerA(std::string s)
  {
    std::transform(s.begin(), s.end(), s.begin(), [](char c) { return (char)std::tolower((unsigned char)c); });
    return s;
  }

  inline fs::path archivePath(const fs::path& dir, const std::string& prefix, int n, const char* ext)
  {
    char name[64];
    snprintf(name, sizeof(name), "%s%04d%s", prefix.c_str(), n, ext);
    return dir / name;
  }

  inline bool readFile(const fs::path& p, std::vector<char>& out)
  {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
  }

  // ---------------------------------------------------------------- archives
  struct Entry
  {
    std::string name;
    uint32_t ts = 0, flags = 0, stored = 0, raw = 0, off = 0;
    fs::path rfd;
  };

  inline std::vector<Entry> readIndex(const fs::path& rfh, const fs::path& rfd)
  {
    std::vector<Entry> out;
    std::vector<char> d;
    if (!readFile(rfh, d)) return out;
    size_t pos = 0;
    while (pos + 24 <= d.size())
    {
      uint32_t v[6];
      memcpy(v, &d[pos], 24);
      pos += 24;
      if (v[0] == 0 || pos + v[0] > d.size()) break;
      Entry e;
      e.name.assign(&d[pos], strnlen(&d[pos], v[0]));
      e.ts = v[1]; e.flags = v[2]; e.stored = v[3]; e.raw = v[4]; e.off = v[5]; e.rfd = rfd;
      out.push_back(e);
      pos += v[0];
    }
    return out;
  }

  inline bool hasEntry(const std::vector<Entry>& idx, const char* name)
  {
    for (const Entry& e : idx) if (lowerA(e.name) == lowerA(name)) return true;
    return false;
  }

  inline bool extract(const Entry& e, std::vector<char>& out)
  {
    std::ifstream f(e.rfd, std::ios::binary);
    if (!f) return false;
    std::vector<char> blob(e.stored);
    f.seekg(e.off);
    if (!f.read(blob.data(), blob.size())) return false;
    if (!(e.flags & 2)) { out.swap(blob); return true; }
    if (blob.size() < 4) return false;
    out.assign(e.raw, 0);
    int n = stbi_zlib_decode_buffer(out.data(), int(out.size()), blob.data() + 4, int(blob.size() - 4));
    return n == int(e.raw);
  }

  // Archives <prefix>0001.. in load order (stops at the first gap, like the game).
  inline std::vector<int> archiveNumbers(const fs::path& data, const std::string& prefix)
  {
    std::vector<int> out;
    std::error_code ec;
    for (int n = 1; n < 100 && fs::exists(archivePath(data, prefix, n, ".RFH"), ec); n++) out.push_back(n);
    return out;
  }

  // Number of our HD archive in DATA, or 0.
  inline int activeHdArchive(const fs::path& data)
  {
    for (int n : archiveNumbers(data, "3DDATA"))
      if (hasEntry(readIndex(archivePath(data, "3DDATA", n, ".RFH"), {}), kHdMarker)) return n;
    return 0;
  }

  // Renumber <prefix> archives so that no gap is left (keeps order).
  inline std::string compact(const fs::path& data, const std::string& prefix)
  {
    std::error_code ec;
    std::vector<int> present;
    int gaps = 0;
    for (int n = 1; n < 100; n++)
    {
      if (fs::exists(archivePath(data, prefix, n, ".RFH"), ec)) present.push_back(n), gaps = 0;
      else if (++gaps > 3) break; // tolerate the single gap we just made
    }
    int target = 1;
    for (int n : present)
    {
      if (n != target)
        for (const char* ext : { ".RFD", ".RFH" }) // index last
        {
          fs::path from = archivePath(data, prefix, n, ext);
          if (fs::exists(from, ec)) fs::rename(from, archivePath(data, prefix, target, ext), ec);
          if (ec) return "rename " + from.filename().string() + " failed";
        }
      target++;
    }
    return "";
  }

  inline fs::path dataDir(const fs::path& gameDir) { return gameDir / "DATA"; }
  inline fs::path cacheDir(const fs::path& gameDir) { return gameDir / "hdtextures"; }
  inline fs::path cacheRfh(const fs::path& gameDir) { return cacheDir(gameDir) / "3DDATA_HD.RFH"; }
  inline fs::path cacheRfd(const fs::path& gameDir) { return cacheDir(gameDir) / "3DDATA_HD.RFD"; }

  inline bool generated(const fs::path& gameDir)
  {
    std::error_code ec;
    return fs::exists(cacheRfh(gameDir), ec) || activeHdArchive(dataDir(gameDir)) != 0;
  }

  inline std::string apply(const fs::path& gameDir, bool enabled)
  {
    std::error_code ec;
    const fs::path data = dataDir(gameDir);
    const int active = activeHdArchive(data);
    if (enabled)
    {
      if (active || !fs::exists(cacheRfh(gameDir), ec)) return "";
      std::vector<int> nums = archiveNumbers(data, "3DDATA");
      int n = nums.empty() ? 1 : nums.back() + 1;
      if (n >= 100) return "no free 3DDATA archive number";
      fs::rename(cacheRfd(gameDir), archivePath(data, "3DDATA", n, ".RFD"), ec);   // data first,
      if (!ec) fs::rename(cacheRfh(gameDir), archivePath(data, "3DDATA", n, ".RFH"), ec); // index last
      return ec ? "move HD archive into DATA: " + ec.message() : "";
    }
    if (!active) return "";
    fs::create_directories(cacheDir(gameDir), ec);
    fs::remove(cacheRfh(gameDir), ec);
    fs::remove(cacheRfd(gameDir), ec);
    fs::rename(archivePath(data, "3DDATA", active, ".RFH"), cacheRfh(gameDir), ec); // index first: game ignores it then
    if (!ec) fs::rename(archivePath(data, "3DDATA", active, ".RFD"), cacheRfd(gameDir), ec);
    if (ec) return "move HD archive out of DATA: " + ec.message();
    return compact(data, "3DDATA");
  }

  // ---------------------------------------------------------------- sorting
  enum Kind { KindSkip, KindIcon, KindModel, KindTerrain };

  inline bool isEffect(const std::string& lowerBase)
  {
    std::string s = lowerBase;
    while (!s.empty() && s[0] == '=') s.erase(0, 1);
    if (!s.empty() && (s[0] == '!' || s[0] == '@' || s[0] == '%')) return true;
    for (const char* k : { "flash", "spotlight", "halo", "beam", "glow", "muzzle" })
      if (s.find(k) != std::string::npos) return true;
    return false;
  }

  // Texture names used by the map terrain meshes (test.xbf: texture name blob after the header).
  inline std::set<std::string> terrainTextureNames(const fs::path& data)
  {
    std::set<std::string> names;
    for (int n : archiveNumbers(data, "MAPS"))
      for (const Entry& e : readIndex(archivePath(data, "MAPS", n, ".RFH"), archivePath(data, "MAPS", n, ".RFD")))
      {
        std::string ln = lowerA(e.name);
        if (ln.size() < 9 || ln.compare(ln.size() - 9, 9, "\\test.xbf") != 0) continue;
        std::vector<char> x;
        if (!extract(e, x) || x.size() < 12) continue;
        int32_t version, fxSize, blobLen;
        memcpy(&version, &x[0], 4); memcpy(&fxSize, &x[4], 4);
        if (version != 1 || fxSize < 0 || size_t(fxSize) + 12 > x.size()) continue;
        memcpy(&blobLen, &x[8 + size_t(fxSize)], 4);
        size_t p = 12 + size_t(fxSize);
        if (blobLen < 0 || p + size_t(blobLen) > x.size()) continue;
        std::string cur;
        for (size_t i = p; i < p + size_t(blobLen); i++)
        {
          char c = x[i];
          if (c == '\0') { if (!cur.empty()) names.insert(lowerA(cur)); cur.clear(); }
          else if (c != '\x02') cur += c;
        }
      }
    return names;
  }

  struct Item
  {
    Entry entry;
    std::string base; // lower case file name, e.g. "at_trike.tga"
    Kind kind = KindSkip;
  };

  // Newest version of every Textures\*.tga in the 3DDATA archives (ours excluded), sorted into recipes.
  inline std::vector<Item> collect(const fs::path& data)
  {
    std::map<std::string, Entry> newest;
    for (int n : archiveNumbers(data, "3DDATA"))
    {
      std::vector<Entry> idx = readIndex(archivePath(data, "3DDATA", n, ".RFH"), archivePath(data, "3DDATA", n, ".RFD"));
      if (hasEntry(idx, kHdMarker) || hasEntry(idx, kModMarker)) continue;
      for (const Entry& e : idx)
      {
        std::string ln = lowerA(e.name);
        if (ln.rfind("textures\\", 0) != 0 || ln.size() < 5 || ln.compare(ln.size() - 4, 4, ".tga") != 0) continue;
        auto it = newest.find(ln);
        if (it == newest.end() || e.ts >= it->second.ts) newest[ln] = e;
      }
    }
    std::set<std::string> bases;
    for (auto& kv : newest) bases.insert(kv.first.substr(9));
    const std::set<std::string> terrain = terrainTextureNames(data);

    std::vector<Item> items;
    for (auto& kv : newest)
    {
      Item it;
      it.entry = kv.second;
      it.base = kv.first.substr(9);
      if (it.base.rfind("grey_", 0) == 0 || bases.count("grey_" + it.base)) it.kind = KindIcon;
      else if (isEffect(it.base)) it.kind = KindSkip;
      else if (terrain.count(it.base)) it.kind = KindTerrain;
      else it.kind = KindModel;
      if (it.kind != KindSkip) items.push_back(it);
    }
    return items;
  }

  // ---------------------------------------------------------------- recipes
  struct Job
  {
    Item item;
    Img original;           // as loaded (3 or 4 channels)
    bool hasAlpha = false;
    std::vector<uint8_t> key; // magenta colour key of the original
    int pad = 0;            // terrain: wrap padding before upscaling
    bool ok = false;
  };

  // Loads the texture and returns the RGB image to send to Real-ESRGAN (false = keep original).
  inline bool prepare(Job& job, Img& toUpscale)
  {
    std::vector<char> raw;
    if (!extract(job.item.entry, raw) || !hdImage::readTga(raw, job.original, job.hasAlpha)) return false;
    const int w = job.original.w, h = job.original.h;
    if (job.item.kind == KindIcon)
    {
      if (w != 64 || h != 64) return false;
      toUpscale = hdImage::descan(hdImage::rgbOf(job.original));
      return true;
    }
    if (std::min(w, h) < 16 || std::max(w, h) > 256) return false;
    Img rgb = hdImage::rgbOf(job.original);
    job.key = hdImage::magentaKey(rgb);
    if (std::count(job.key.begin(), job.key.end(), 1) == int(job.key.size())) return false; // fully transparent
    hdImage::fillMasked(rgb, job.key);
    if (job.item.kind == KindTerrain)
    {
      job.pad = std::max(8, w / 4);
      toUpscale = hdImage::padWrap(rgb, job.pad);
    }
    else
      toUpscale = rgb;
    return true;
  }

  // Takes Real-ESRGAN's 4x result and returns the final texture.
  inline Img finish(const Job& job, const Img& up4)
  {
    const int w = job.original.w, h = job.original.h;
    if (job.item.kind == KindIcon)
    {
      Img out = hdImage::resizeLanczos(up4, 128, 128);
      hdImage::clampTo8bit(out);
      return out;
    }
    Img rgb;
    int fw, fh;
    if (job.item.kind == KindTerrain)
    {
      rgb = hdImage::crop(up4, 4 * job.pad, 4 * job.pad, 4 * w, 4 * h);
      rgb = hdImage::dampSandLines(rgb);
      fw = 4 * w; fh = 4 * h;
    }
    else
    {
      Img r = hdImage::remaster(up4);
      // keep transparent texels out of the downscale filter
      std::vector<uint8_t> key4 = hdImage::scaleMask(job.key, w, h, r.w, r.h);
      hdImage::fillMasked(r, key4);
      fw = 2 * w; fh = 2 * h;
      rgb = hdImage::resizeLanczos(r, fw, fh);
      hdImage::clampTo8bit(rgb);
      hdImage::restoreHouseColour(rgb, job.original);
    }
    std::vector<uint8_t> keyF = hdImage::scaleMask(job.key, w, h, fw, fh);
    for (size_t i = 0; i < keyF.size(); i++)
      if (keyF[i]) { rgb.d[i * 3] = 255; rgb.d[i * 3 + 1] = 0; rgb.d[i * 3 + 2] = 255; }
    if (!job.hasAlpha) return rgb;
    Img alpha(w, h, 1);
    for (size_t i = 0; i < size_t(w) * h; i++) alpha.d[i] = job.original.d[i * 4 + 3];
    alpha = hdImage::resizeLanczos(alpha, fw, fh);
    Img out(fw, fh, 4);
    for (size_t i = 0; i < size_t(fw) * fh; i++)
    {
      for (int k = 0; k < 3; k++) out.d[i * 4 + k] = rgb.d[i * 3 + k];
      out.d[i * 4 + 3] = std::min(255.0f, std::max(0.0f, std::round(alpha.d[i])));
    }
    return out;
  }

  // ---------------------------------------------------------------- generate
  struct Progress
  {
    std::function<void(int stage, int done, int total)> report;
    std::function<bool()> cancelled;
  };

  // Upscales every PNG in inDir 4x into outDir (same file names). Calls poll() regularly; poll()
  // returning false means: stop now. Returns "" or an error text.
  using Runner = std::function<std::string(const fs::path& inDir, const fs::path& outDir, const std::function<bool()>& poll)>;

  // PNG via memory, so that paths with any characters work (stb's own fopen is 8-bit only).
  inline bool writePng(const fs::path& p, const Img& im)
  {
    std::vector<uint8_t> px(im.d.size());
    for (size_t i = 0; i < px.size(); i++) px[i] = uint8_t(std::min(255.0f, std::max(0.0f, std::round(im.d[i]))));
    std::vector<char> bytes;
    auto sink = [](void* ctx, void* data, int size) {
      auto* v = static_cast<std::vector<char>*>(ctx);
      v->insert(v->end(), static_cast<char*>(data), static_cast<char*>(data) + size);
    };
    if (!stbi_write_png_to_func(sink, &bytes, im.w, im.h, im.c, px.data(), im.w * im.c)) return false;
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(bytes.data(), bytes.size());
    return bool(f);
  }

  inline bool readPng(const fs::path& p, Img& im)
  {
    std::vector<char> bytes;
    if (!readFile(p, bytes)) return false;
    int w, h, c;
    unsigned char* px = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()), int(bytes.size()), &w, &h, &c, 3);
    if (!px) return false;
    im = Img(w, h, 3);
    for (size_t i = 0; i < im.d.size(); i++) im.d[i] = px[i];
    stbi_image_free(px);
    return true;
  }

  // stage: 0 reading, 1 upscaling, 2 packing
  inline std::string generate(const fs::path& gameDir, const fs::path& workDir, const Runner& run, const Progress& progress,
                              int batchSize = 48, int threads = 4, const std::set<std::string>* only = nullptr)
  {
    std::error_code ec;
    const fs::path data = dataDir(gameDir);
    if (!fs::is_directory(data, ec)) return "DATA folder not found";
    progress.report(0, 0, 0);

    // an old result must not be read as game data, and gets replaced anyway
    std::string err = apply(gameDir, false);
    if (!err.empty()) return err;
    fs::remove(cacheRfh(gameDir), ec);
    fs::remove(cacheRfd(gameDir), ec);
    fs::create_directories(cacheDir(gameDir), ec);

    std::vector<Item> items = collect(data);
    if (only)
      items.erase(std::remove_if(items.begin(), items.end(), [&](const Item& i) { return !only->count(i.base); }), items.end());
    if (items.empty()) return "no textures found in the 3DDATA archives";

    const fs::path tmpRfd = cacheDir(gameDir) / "3DDATA_HD.RFD.tmp";
    std::ofstream rfd(tmpRfd, std::ios::binary | std::ios::trunc);
    if (!rfd) return "cannot write " + tmpRfd.string();
    std::vector<char> rfh;
    uint32_t offset = 0;
    const uint32_t now = uint32_t(time(nullptr));
    std::mutex writeLock;
    auto put32 = [&](uint32_t x) { rfh.insert(rfh.end(), reinterpret_cast<char*>(&x), reinterpret_cast<char*>(&x) + 4); };
    auto addEntry = [&](const std::string& name, const std::vector<char>& content) {
      put32(uint32_t(name.size() + 1)); put32(now); put32(0); put32(uint32_t(content.size())); put32(uint32_t(content.size())); put32(offset);
      rfh.insert(rfh.end(), name.begin(), name.end()); rfh.push_back('\0');
      rfd.write(content.data(), content.size());
      offset += uint32_t(content.size());
    };

    const fs::path inDir = workDir / "in", outDir = workDir / "out";
    int done = 0, written = 0;
    const int total = int(items.size());
    auto cleanup = [&] { fs::remove_all(workDir, ec); };
    auto failed = [&](const std::string& msg) { rfd.close(); fs::remove(tmpRfd, ec); cleanup(); return msg; };

    for (size_t b0 = 0; b0 < items.size(); b0 += size_t(batchSize))
    {
      if (progress.cancelled()) return failed("cancelled");
      fs::remove_all(workDir, ec);
      fs::create_directories(inDir, ec);
      fs::create_directories(outDir, ec);
      const size_t b1 = std::min(items.size(), b0 + size_t(batchSize));
      std::vector<Job> jobs(b1 - b0);
      for (size_t i = b0; i < b1; i++)
      {
        Job& job = jobs[i - b0];
        job.item = items[i];
        Img up;
        char name[32];
        snprintf(name, sizeof(name), "%05d.png", int(i));
        job.ok = prepare(job, up) && writePng(inDir / name, up);
      }

      progress.report(1, done, total);
      err = run(inDir, outDir, [&] {
        int n = 0;
        for (auto& e : fs::directory_iterator(outDir, ec)) { (void)e; n++; }
        progress.report(1, std::min(total - 1, done + n), total);
        return !progress.cancelled();
      });
      if (progress.cancelled()) return failed("cancelled");
      if (!err.empty()) return failed(err);

      // finishing in parallel, writing in order of completion
      std::atomic<size_t> next{ 0 };
      std::string threadError;
      auto worker = [&] {
        for (size_t j; (j = next++) < jobs.size();)
        {
          if (progress.cancelled()) return;
          Job& job = jobs[j];
          if (!job.ok) continue;
          char name[32];
          snprintf(name, sizeof(name), "%05d.png", int(b0 + j));
          Img up4;
          if (!readPng(outDir / name, up4))
          {
            std::lock_guard<std::mutex> g(writeLock);
            if (threadError.empty()) threadError = "Real-ESRGAN produced no result for " + job.item.entry.name;
            return;
          }
          std::vector<char> tga = hdImage::writeTga(finish(job, up4));
          std::lock_guard<std::mutex> g(writeLock);
          addEntry(job.item.entry.name, tga);
          written++;
        }
      };
      std::vector<std::thread> pool;
      for (int t = 0; t < std::max(1, threads); t++) pool.emplace_back(worker);
      for (auto& t : pool) t.join();
      if (progress.cancelled()) return failed("cancelled");
      if (!threadError.empty()) return failed(threadError);
      if (!rfd) return failed("cannot write " + tmpRfd.string());
      done = int(b1);
      progress.report(1, done, total);
    }

    progress.report(2, total, total);
    std::string marker = std::string("Emperor Spice: ") + kRecipeVersion +
                         ". Made by the launcher from your own game files - do not share.\r\n";
    addEntry(kHdMarker, std::vector<char>(marker.begin(), marker.end()));
    rfd.close();
    if (!rfd) return failed("cannot write " + tmpRfd.string());
    {
      std::ofstream f(cacheRfh(gameDir), std::ios::binary | std::ios::trunc);
      f.write(rfh.data(), rfh.size());
      if (!f) return failed("cannot write " + cacheRfh(gameDir).string());
    }
    fs::rename(tmpRfd, cacheRfd(gameDir), ec);
    if (ec) { fs::remove(cacheRfh(gameDir), ec); return failed("cannot rename " + tmpRfd.string()); }
    cleanup();
    if (written == 0) return "no texture could be converted";
    return "";
  }
}
