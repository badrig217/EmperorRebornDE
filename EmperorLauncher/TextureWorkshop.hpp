#pragma once
// Emperor Spice "Grafik-Werkstatt": HD textures made on the player's PC from the player's own game
// files (EA data must not be shipped). Recipes were tuned in game tests (docs/PROJECT-STATUS.md):
//  - build icons (64x64, colour + grey):  remove scanlines, Real-ESRGAN 4x, downscale to 128
//  - unit/building textures:              Real-ESRGAN 4x, "remaster" (relief, dust, worn edges, grain),
//                                         downscale to 2x, house-colour blue restored exactly
//  - terrain textures (used by the maps): Real-ESRGAN 4x seamless (wrap padding), no remaster,
//                                         thin dark lines in smooth sand damped
//  - effect textures (flashes, lights ...) stay original
// Real-ESRGAN runs as realesrgan-ncnn-vulkan.exe (Vulkan, any modern GPU) from the "hdtools" folder.
//
// The result is one extra 3DDATA archive with the marker entry "EmperorSpice\HDTextures.txt".
// Switched on, it sits in DATA at the next free number (its entries are older than mod entries, so
// mods still win). Switched off, it is moved to <game>\hdtextures\ and the archives above it are
// renumbered, so the game sees no gap.
#include <atomic>
#include <filesystem>
#include <string>

namespace hdTextures
{
  namespace fs = std::filesystem;

  enum Stage { StageReading = 0, StagePreparing, StageUpscaling, StagePacking, StageDone };

  struct Progress
  {
    std::atomic<int> stage{ StageReading };
    std::atomic<int> done{ 0 };
    std::atomic<int> total{ 0 };
    std::atomic<bool> cancel{ false };
  };

  // toolDir contains realesrgan-ncnn-vulkan.exe and models\realesrgan-x4plus.{param,bin}.
  bool toolAvailable(const fs::path& toolDir);

  // true if an HD archive exists (in DATA or in the cache folder).
  bool generated(const fs::path& gameDir);

  // Builds the HD archive into <gameDir>\hdtextures. Returns L"" on success, L"cancelled" if
  // progress.cancel was set, otherwise an error description. Runs for minutes: call from a worker thread.
  std::wstring generate(const fs::path& toolDir, const fs::path& gameDir, Progress& progress);

  // Puts the HD archive into DATA (enabled) or moves it out to the cache folder (disabled).
  std::wstring apply(const fs::path& gameDir, bool enabled);
}
