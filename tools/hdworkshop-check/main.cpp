// Runs the launcher's HD texture workshop core on Linux with the Linux build of realesrgan-ncnn-vulkan
// (same model as the Windows build), to check results against the in-game tested prototypes.
// build: g++ -std=c++20 -O2 -pthread -I../../EmperorLauncher main.cpp -o hdcheck
// usage: ./hdcheck GAME_DIR WORK_DIR REALESRGAN_DIR [texture.tga ...]   (GAME_DIR contains DATA\)
//        ./hdcheck GAME_DIR on|off   (switch the generated archive on/off)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "HdWorkshopCore.hpp"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
  if (argc == 3 && (std::string(argv[2]) == "on" || std::string(argv[2]) == "off"))
  {
    std::string e = hdCore::apply(argv[1], std::string(argv[2]) == "on");
    fprintf(stderr, "apply: '%s', generated %d, active %d\n", e.c_str(), hdCore::generated(argv[1]), hdCore::activeHdArchive(hdCore::dataDir(argv[1])));
    return e.empty() ? 0 : 1;
  }
  if (argc < 4) { fprintf(stderr, "usage: hdcheck GAME_DIR WORK_DIR REALESRGAN_DIR [names]\n"); return 2; }
  std::set<std::string> only;
  for (int i = 4; i < argc; i++) only.insert(hdCore::lowerA(argv[i]));
  std::string tool = argv[3];
  hdCore::Runner run = [&](const hdCore::fs::path& in, const hdCore::fs::path& out, const std::function<bool()>& poll) -> std::string {
    std::string cmd = "export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json; cd '" + tool + "' && ./realesrgan-ncnn-vulkan -i '" + in.string() + "' -o '" + out.string() +
                      "' -n realesrgan-x4plus -s 4 -f png > /dev/null 2>&1";
    poll();
    int rc = system(cmd.c_str());
    poll();
    return rc == 0 ? "" : "realesrgan failed";
  };
  hdCore::Progress p;
  p.report = [](int stage, int done, int total) { fprintf(stderr, "\rstage %d  %d/%d   ", stage, done, total); };
  p.cancelled = [] { return false; };
  auto items = hdCore::collect(hdCore::dataDir(argv[1]));
  int c[4] = {};
  for (auto& i : items) c[i.kind]++;
  fprintf(stderr, "textures: icons %d, models %d, terrain %d\n", c[hdCore::KindIcon], c[hdCore::KindModel], c[hdCore::KindTerrain]);
  std::string err = hdCore::generate(argv[1], argv[2], run, p, 48, 4, only.empty() ? nullptr : &only);
  fprintf(stderr, "\nresult: '%s'\n", err.c_str());
  return err.empty() ? 0 : 1;
}
