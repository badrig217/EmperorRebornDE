#pragma once
// Image helpers for the HD texture workshop (platform independent, header only).
// Ports of the prototype filters (numpy/scipy) used to tune the recipes; see TextureWorkshop.hpp.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace hdImage
{
  // Float image, values 0..255, interleaved channels, row 0 = top.
  struct Img
  {
    int w = 0, h = 0, c = 0;
    std::vector<float> d;
    Img() = default;
    Img(int w_, int h_, int c_, float v = 0) : w(w_), h(h_), c(c_), d(size_t(w_) * h_ * c_, v) {}
    float& at(int x, int y, int k) { return d[(size_t(y) * w + x) * c + k]; }
    float at(int x, int y, int k) const { return d[(size_t(y) * w + x) * c + k]; }
  };

  // Single channel float plane.
  struct Plane
  {
    int w = 0, h = 0;
    std::vector<float> d;
    Plane() = default;
    Plane(int w_, int h_, float v = 0) : w(w_), h(h_), d(size_t(w_) * h_, v) {}
    float& at(int x, int y) { return d[size_t(y) * w + x]; }
    float at(int x, int y) const { return d[size_t(y) * w + x]; }
  };

  inline int wrapi(int i, int n) { i %= n; return i < 0 ? i + n : i; }
  inline int clampi(int i, int n) { return i < 0 ? 0 : (i >= n ? n - 1 : i); }

  inline Plane channel(const Img& im, int k)
  {
    Plane p(im.w, im.h);
    for (size_t i = 0; i < p.d.size(); i++) p.d[i] = im.d[i * im.c + k];
    return p;
  }

  inline void setChannel(Img& im, int k, const Plane& p)
  {
    for (size_t i = 0; i < p.d.size(); i++) im.d[i * im.c + k] = p.d[i];
  }

  inline Img rgbOf(const Img& im)
  {
    Img o(im.w, im.h, 3);
    for (size_t i = 0; i < size_t(im.w) * im.h; i++)
      for (int k = 0; k < 3; k++) o.d[i * 3 + k] = im.d[i * im.c + k];
    return o;
  }

  // ---------------------------------------------------------------- separable filters
  // 1D pass over rows (horizontal) or columns (vertical) with a kernel; wrap or nearest border.
  inline Plane convolve1d(const Plane& src, const std::vector<float>& kernel, bool horizontal, bool wrap)
  {
    Plane out(src.w, src.h);
    const int r = int(kernel.size() / 2);
    for (int y = 0; y < src.h; y++)
      for (int x = 0; x < src.w; x++)
      {
        float s = 0;
        for (int i = -r; i <= r; i++)
        {
          int xx = horizontal ? x + i : x, yy = horizontal ? y : y + i;
          if (wrap) { xx = wrapi(xx, src.w); yy = wrapi(yy, src.h); }
          else { xx = clampi(xx, src.w); yy = clampi(yy, src.h); }
          s += kernel[size_t(i + r)] * src.at(xx, yy);
        }
        out.at(x, y) = s;
      }
    return out;
  }

  // scipy.ndimage.gaussian_filter (truncate 4.0)
  inline Plane gaussian(const Plane& src, float sigma, bool wrap)
  {
    if (sigma <= 0) return src;
    const int r = int(4.0f * sigma + 0.5f);
    std::vector<float> k(size_t(2 * r + 1));
    float sum = 0;
    for (int i = -r; i <= r; i++) { k[size_t(i + r)] = std::exp(-0.5f * i * i / (sigma * sigma)); sum += k[size_t(i + r)]; }
    for (float& v : k) v /= sum;
    return convolve1d(convolve1d(src, k, true, wrap), k, false, wrap);
  }

  // Box mean of the given size (scipy.ndimage.uniform_filter, wrap).
  inline Plane boxMean(const Plane& src, int size)
  {
    size |= 1; // odd, centred window
    std::vector<float> k(size_t(size), 1.0f / size);
    return convolve1d(convolve1d(src, k, true, true), k, false, true);
  }

  // Grey dilation/erosion with a square of side `size` (wrap).
  inline Plane minMax(const Plane& src, int size, bool takeMax)
  {
    const int r = size / 2;
    Plane tmp(src.w, src.h), out(src.w, src.h);
    for (int y = 0; y < src.h; y++)
      for (int x = 0; x < src.w; x++)
      {
        float v = src.at(wrapi(x - r, src.w), y);
        for (int i = -r + 1; i <= r; i++) { float t = src.at(wrapi(x + i, src.w), y); v = takeMax ? std::max(v, t) : std::min(v, t); }
        tmp.at(x, y) = v;
      }
    for (int y = 0; y < src.h; y++)
      for (int x = 0; x < src.w; x++)
      {
        float v = tmp.at(x, wrapi(y - r, src.h));
        for (int i = -r + 1; i <= r; i++) { float t = tmp.at(x, wrapi(y + i, src.h)); v = takeMax ? std::max(v, t) : std::min(v, t); }
        out.at(x, y) = v;
      }
    return out;
  }

  inline Plane greyClosing(const Plane& src, int size) { return minMax(minMax(src, size, true), size, false); }

  // ---------------------------------------------------------------- resampling
  inline double lanczos3(double x)
  {
    x = std::fabs(x);
    if (x < 1e-8) return 1.0;
    if (x >= 3.0) return 0.0;
    const double pi = 3.14159265358979323846;
    return 3.0 * std::sin(pi * x) * std::sin(pi * x / 3.0) / (pi * pi * x * x);
  }

  // PIL-style Lanczos (antialiased when shrinking), one axis.
  inline Img resampleAxis(const Img& src, int newSize, bool horizontal)
  {
    const int oldSize = horizontal ? src.w : src.h;
    const double scale = double(newSize) / oldSize;
    const double filterScale = scale < 1.0 ? 1.0 / scale : 1.0;
    const double support = 3.0 * filterScale;
    Img out(horizontal ? newSize : src.w, horizontal ? src.h : newSize, src.c);
    std::vector<double> wts;
    for (int i = 0; i < newSize; i++)
    {
      const double center = (i + 0.5) / scale;
      int lo = std::max(0, int(std::floor(center - support)));
      int hi = std::min(oldSize, int(std::ceil(center + support)));
      wts.assign(size_t(hi - lo), 0.0);
      double sum = 0;
      for (int j = lo; j < hi; j++) { double wv = lanczos3((j + 0.5 - center) / filterScale); wts[size_t(j - lo)] = wv; sum += wv; }
      if (sum == 0) sum = 1;
      const int other = horizontal ? src.h : src.w;
      for (int o = 0; o < other; o++)
        for (int k = 0; k < src.c; k++)
        {
          double s = 0;
          for (int j = lo; j < hi; j++)
            s += wts[size_t(j - lo)] * (horizontal ? src.at(j, o, k) : src.at(o, j, k));
          float v = float(s / sum);
          if (horizontal) out.at(i, o, k) = v; else out.at(o, i, k) = v;
        }
    }
    return out;
  }

  inline Img resizeLanczos(const Img& src, int w, int h)
  {
    Img t = (w == src.w) ? src : resampleAxis(src, w, true);
    return (h == src.h) ? t : resampleAxis(t, h, false);
  }

  inline void clampTo8bit(Img& im)
  {
    for (float& v : im.d) v = std::round(std::min(255.0f, std::max(0.0f, v)));
  }

  // Nearest-neighbour scaling of a mask (as PIL NEAREST).
  inline std::vector<uint8_t> scaleMask(const std::vector<uint8_t>& m, int w, int h, int nw, int nh)
  {
    std::vector<uint8_t> o(size_t(nw) * nh);
    for (int y = 0; y < nh; y++)
      for (int x = 0; x < nw; x++)
        o[size_t(y) * nw + x] = m[size_t(std::min(h - 1, int((y + 0.5) * h / nh))) * w + std::min(w - 1, int((x + 0.5) * w / nw))];
    return o;
  }

  // ---------------------------------------------------------------- recipe building blocks
  // Colour key used by the game for transparent texels.
  inline std::vector<uint8_t> magentaKey(const Img& im)
  {
    std::vector<uint8_t> m(size_t(im.w) * im.h, 0);
    for (size_t i = 0; i < m.size(); i++)
      m[i] = im.d[i * im.c] == 255 && im.d[i * im.c + 1] == 0 && im.d[i * im.c + 2] == 255;
    return m;
  }

  // Replace masked pixels with the colour of the nearest unmasked pixel (multi-source BFS).
  inline void fillMasked(Img& im, const std::vector<uint8_t>& mask)
  {
    const size_t n = size_t(im.w) * im.h;
    std::vector<int> src(n, -1), queue;
    for (size_t i = 0; i < n; i++) if (!mask[i]) { src[i] = int(i); queue.push_back(int(i)); }
    if (queue.empty()) return;
    for (size_t q = 0; q < queue.size(); q++)
    {
      int i = queue[q], x = i % im.w, y = i / im.w;
      const int nx[4] = { x - 1, x + 1, x, x }, ny[4] = { y, y, y - 1, y + 1 };
      for (int t = 0; t < 4; t++)
      {
        if (nx[t] < 0 || ny[t] < 0 || nx[t] >= im.w || ny[t] >= im.h) continue;
        int j = ny[t] * im.w + nx[t];
        if (src[size_t(j)] >= 0) continue;
        src[size_t(j)] = src[size_t(i)];
        queue.push_back(j);
      }
    }
    for (size_t i = 0; i < n; i++)
      if (mask[i])
        for (int k = 0; k < im.c; k++) im.d[i * im.c + k] = im.d[size_t(src[i]) * im.c + k];
  }

  // Icons have dark scanlines every second row: vertical [1,2,1]/4 removes them completely.
  inline Img descan(const Img& im)
  {
    Img o(im.w, im.h, im.c);
    for (int y = 0; y < im.h; y++)
      for (int x = 0; x < im.w; x++)
        for (int k = 0; k < im.c; k++)
          o.at(x, y, k) = (im.at(x, clampi(y - 1, im.h), k) + 2 * im.at(x, y, k) + im.at(x, clampi(y + 1, im.h), k)) / 4;
    return o;
  }

  inline Img padWrap(const Img& im, int p)
  {
    Img o(im.w + 2 * p, im.h + 2 * p, im.c);
    for (int y = 0; y < o.h; y++)
      for (int x = 0; x < o.w; x++)
        for (int k = 0; k < im.c; k++) o.at(x, y, k) = im.at(wrapi(x - p, im.w), wrapi(y - p, im.h), k);
    return o;
  }

  inline Img crop(const Img& im, int x0, int y0, int w, int h)
  {
    Img o(w, h, im.c);
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
        for (int k = 0; k < im.c; k++) o.at(x, y, k) = im.at(x0 + x, y0 + y, k);
    return o;
  }

  // "Remaster dezent": fake relief, dust in cavities, worn edges, fine grain; house-colour blue untouched.
  // Port of tools prototype remaster.py (relief 0.45, dust 0.6, wear 0.6, grain 0.8).
  inline Img remaster(const Img& rgbIn, float relief = 0.45f, float dust = 0.6f, float wear = 0.6f, float grain = 0.8f)
  {
    const int W = rgbIn.w, H = rgbIn.h;
    const float s = W / 512.0f;
    Plane lum(W, H);
    for (size_t i = 0; i < lum.d.size(); i++)
      lum.d[i] = (rgbIn.d[i * 3] * 0.3f + rgbIn.d[i * 3 + 1] * 0.59f + rgbIn.d[i * 3 + 2] * 0.11f) / 255.0f;
    Plane a1 = gaussian(lum, 1.2f * s, true), a2 = gaussian(lum, 14.0f * s, true);
    Plane hgt(W, H);
    for (size_t i = 0; i < hgt.d.size(); i++) hgt.d[i] = a1.d[i] - a2.d[i];
    Plane blurH = gaussian(hgt, 4.0f * s, true);

    std::mt19937 rng(1);
    std::normal_distribution<float> nd(0.0f, 1.0f);
    Plane noise(W, H), blotch(W, H);
    for (float& v : noise.d) v = nd(rng);
    for (float& v : blotch.d) v = nd(rng);
    noise = gaussian(noise, 0.7f * s, true);
    blotch = gaussian(blotch, 10.0f * s, true);
    double mean = 0, var = 0;
    for (float v : blotch.d) mean += v;
    mean /= blotch.d.size();
    for (float v : blotch.d) var += (v - mean) * (v - mean);
    const float sd = float(std::sqrt(var / blotch.d.size())) + 1e-6f;

    // house colour: saturated blue, dilated, softened
    std::vector<uint8_t> team(size_t(W) * H);
    for (size_t i = 0; i < team.size(); i++)
    {
      float r = rgbIn.d[i * 3] / 255, g = rgbIn.d[i * 3 + 1] / 255, b = rgbIn.d[i * 3 + 2] / 255;
      team[i] = b > r * 1.6f && b > g * 1.4f && b > 0.35f;
    }
    for (int it = 0; it < std::max(1, int(s)); it++)
    {
      std::vector<uint8_t> t2 = team;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          if (!team[size_t(y) * W + x])
            t2[size_t(y) * W + x] = (x > 0 && team[size_t(y) * W + x - 1]) || (x + 1 < W && team[size_t(y) * W + x + 1]) ||
                                    (y > 0 && team[size_t(y - 1) * W + x]) || (y + 1 < H && team[size_t(y + 1) * W + x]);
      team.swap(t2);
    }
    Plane teamP(W, H);
    for (size_t i = 0; i < team.size(); i++) teamP.d[i] = team[i];
    teamP = gaussian(teamP, 0.8f * s, true);

    const float L0 = -0.5f, L1 = -0.6f, L2 = 0.62f, Ln = std::sqrt(L0 * L0 + L1 * L1 + L2 * L2);
    const float lx = L0 / Ln, ly = L1 / Ln, lz = L2 / Ln;
    const float nz = 0.12f / std::max(s, 0.25f);
    const float dustCol[3] = { 0.62f, 0.48f, 0.33f };
    Img out(W, H, 3);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
      {
        // np.gradient: central differences, one-sided at the borders
        auto hv = [&](int xx, int yy) { return hgt.at(xx, yy) * 6.0f; };
        float gx = (x == 0) ? hv(1, y) - hv(0, y) : (x == W - 1) ? hv(W - 1, y) - hv(W - 2, y) : (hv(x + 1, y) - hv(x - 1, y)) / 2;
        float gy = (y == 0) ? hv(x, 1) - hv(x, 0) : (y == H - 1) ? hv(x, H - 1) - hv(x, H - 2) : (hv(x, y + 1) - hv(x, y - 1)) / 2;
        float n0 = -gx, n1 = -gy, n2 = nz, nl = std::sqrt(n0 * n0 + n1 * n1 + n2 * n2);
        float shade = (n0 * lx + n1 * ly + n2 * lz) / nl - lz;
        float curv = hgt.at(x, y) - blurH.at(x, y);
        float cavity = std::min(1.0f, std::max(0.0f, -curv * 9));
        float edge = std::min(1.0f, std::max(0.0f, curv * 9));
        float bl = std::min(0.2f, std::max(0.0f, blotch.at(x, y) / sd * 0.12f));
        float dmask = std::min(1.0f, std::max(0.0f, cavity * 0.75f + bl)) * dust * 0.55f;
        float o[3], a[3];
        for (int k = 0; k < 3; k++) { a[k] = rgbIn.at(x, y, k) / 255; o[k] = a[k] * (1 + relief * 0.9f * shade); }
        float m = (o[0] + o[1] + o[2]) / 3;
        float t = teamP.at(x, y);
        for (int k = 0; k < 3; k++)
        {
          float v = o[k] * (1 - dmask) + dustCol[k] * dmask * (0.6f + 0.4f * m);
          v *= 1 - 0.35f * cavity * dust;
          v += wear * 0.18f * edge * (1.1f - v);
          v *= 1 + grain * 0.035f * noise.at(x, y);
          v = v * (1 - t) + a[k] * t;
          out.at(x, y, k) = std::min(1.0f, std::max(0.0f, v)) * 255;
        }
      }
    clampTo8bit(out);
    return out;
  }

  // Thin dark lines inside smooth sand areas (cracks drawn into sand ramps) become a visible grid when
  // the texture is tiled. Dampen them; rock, cliffs and ice stay untouched. Port of sandfix.py.
  inline Img dampSandLines(const Img& rgb)
  {
    const int W = rgb.w, H = rgb.h, k = std::max(3, W / 64), sz = 2 * k + 1;
    Plane lum(W, H);
    for (size_t i = 0; i < lum.d.size(); i++) lum.d[i] = (rgb.d[i * 3] + rgb.d[i * 3 + 1] + rgb.d[i * 3 + 2]) / 3;
    Plane closed = greyClosing(lum, sz);
    Plane sq(W, H);
    for (size_t i = 0; i < sq.d.size(); i++) sq.d[i] = closed.d[i] * closed.d[i];
    Plane m1 = boxMean(closed, 6 * k), m2 = boxMean(sq, 6 * k);
    Plane cl[3], fill[3];
    for (int c = 0; c < 3; c++) { cl[c] = greyClosing(channel(rgb, c), sz); fill[c] = gaussian(cl[c], 1.5f, true); }
    Plane region(W, H);
    for (size_t i = 0; i < region.d.size(); i++)
    {
      float sd = std::sqrt(std::max(0.0f, m2.d[i] - m1.d[i] * m1.d[i]));
      bool warm = cl[0].d[i] > cl[2].d[i] * 1.25f && closed.d[i] > 140;
      region.d[i] = (sd < 9 && warm) ? 1.0f : 0.0f;
    }
    region = gaussian(region, float(k), true);
    Plane mask(W, H);
    for (size_t i = 0; i < mask.d.size(); i++)
      mask.d[i] = std::min(1.0f, std::max(0.0f, (closed.d[i] - lum.d[i] - 3) / 15)) * region.d[i];
    mask = gaussian(mask, 1.0f, true);
    Img out = rgb;
    for (size_t i = 0; i < mask.d.size(); i++)
      for (int c = 0; c < 3; c++) out.d[i * 3 + c] = rgb.d[i * 3 + c] * (1 - mask.d[i]) + fill[c].d[i] * mask.d[i];
    clampTo8bit(out);
    return out;
  }

  // Pixels that are pure blue (R=G=0) in the original are the house colour: the game recolours them
  // per player. After upscaling they must be pure blue again, snapped to the original's blue levels.
  inline void restoreHouseColour(Img& rgb, const Img& original)
  {
    std::vector<uint8_t> m(size_t(original.w) * original.h);
    std::set<int> levelSet;
    for (size_t i = 0; i < m.size(); i++)
    {
      m[i] = original.d[i * original.c] == 0 && original.d[i * original.c + 1] == 0 && original.d[i * original.c + 2] > 0;
      if (m[i]) levelSet.insert(int(original.d[i * original.c + 2]));
    }
    if (levelSet.empty()) return;
    std::vector<int> levels(levelSet.begin(), levelSet.end());
    std::vector<uint8_t> mm = scaleMask(m, original.w, original.h, rgb.w, rgb.h);
    for (size_t i = 0; i < mm.size(); i++)
    {
      if (!mm[i]) continue;
      float b = rgb.d[i * rgb.c + 2];
      int best = levels[0];
      for (int l : levels) if (std::fabs(l - b) < std::fabs(best - b)) best = l;
      rgb.d[i * rgb.c] = 0; rgb.d[i * rgb.c + 1] = 0; rgb.d[i * rgb.c + 2] = float(best);
    }
  }

  // ---------------------------------------------------------------- TGA
  // Reads uncompressed true-colour TGA (type 2; 16/24/32 bit). 16-bit is RGB555 (the game's
  // files never use the attribute bit). Returns false for other formats (texture is then skipped).
  inline bool readTga(const std::vector<char>& f, Img& im, bool& hasAlpha)
  {
    if (f.size() < 18) return false;
    const uint8_t* d = reinterpret_cast<const uint8_t*>(f.data());
    const int idLen = d[0], cmType = d[1], type = d[2];
    const int w = d[12] | d[13] << 8, h = d[14] | d[15] << 8, bpp = d[16], desc = d[17];
    if (type != 2 || cmType != 0 || w <= 0 || h <= 0 || (bpp != 16 && bpp != 24 && bpp != 32)) return false;
    const size_t off = 18 + size_t(idLen), bytes = size_t(bpp / 8);
    if (f.size() < off + size_t(w) * h * bytes) return false;
    hasAlpha = bpp == 32;
    im = Img(w, h, hasAlpha ? 4 : 3);
    const bool topDown = (desc & 0x20) != 0;
    for (int y = 0; y < h; y++)
    {
      const int row = topDown ? y : h - 1 - y;
      for (int x = 0; x < w; x++)
      {
        const uint8_t* p = d + off + (size_t(y) * w + x) * bytes;
        float r, g, b, a = 255;
        if (bpp == 16)
        {
          int v = p[0] | p[1] << 8;
          r = float(((v >> 10) & 31) * 255 / 31); g = float(((v >> 5) & 31) * 255 / 31); b = float((v & 31) * 255 / 31);
        }
        else { b = p[0]; g = p[1]; r = p[2]; if (bpp == 32) a = p[3]; }
        im.at(x, row, 0) = r; im.at(x, row, 1) = g; im.at(x, row, 2) = b;
        if (hasAlpha) im.at(x, row, 3) = a;
      }
    }
    return true;
  }

  // Writes an uncompressed TGA (type 2), bottom-up like the game's own files. 3 or 4 channels.
  inline std::vector<char> writeTga(const Img& im)
  {
    const int bpp = im.c == 4 ? 32 : 24, bytes = bpp / 8;
    std::vector<char> f(18 + size_t(im.w) * im.h * bytes, 0);
    uint8_t* d = reinterpret_cast<uint8_t*>(f.data());
    d[2] = 2;
    d[12] = uint8_t(im.w); d[13] = uint8_t(im.w >> 8); d[14] = uint8_t(im.h); d[15] = uint8_t(im.h >> 8);
    d[16] = uint8_t(bpp); d[17] = im.c == 4 ? 8 : 0;
    for (int y = 0; y < im.h; y++)
      for (int x = 0; x < im.w; x++)
      {
        uint8_t* p = d + 18 + (size_t(im.h - 1 - y) * im.w + x) * bytes;
        auto q = [&](int k) { return uint8_t(std::min(255.0f, std::max(0.0f, std::round(im.at(x, y, k))))); };
        p[0] = q(2); p[1] = q(1); p[2] = q(0);
        if (bytes == 4) p[3] = q(3);
      }
    return f;
  }
}
