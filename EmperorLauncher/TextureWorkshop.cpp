// Grafik-Werkstatt: Windows side (runs realesrgan-ncnn-vulkan.exe). Recipes and archives: HdWorkshopCore.hpp.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG // we only read PNG (Real-ESRGAN output) and zlib (game archives)
#include "TextureWorkshop.hpp"
#include "HdWorkshopCore.hpp"

namespace hdTextures
{
  static const wchar_t kExe[] = L"realesrgan-ncnn-vulkan.exe";

  bool toolAvailable(const fs::path& toolDir)
  {
    std::error_code ec;
    return fs::exists(toolDir / kExe, ec) && fs::exists(toolDir / L"models" / L"realesrgan-x4plus.param", ec) &&
           fs::exists(toolDir / L"models" / L"realesrgan-x4plus.bin", ec);
  }

  bool generated(const fs::path& gameDir) { return hdCore::generated(gameDir); }

  static std::wstring widen(const std::string& s)
  {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), int(s.size()), w.data(), n);
    return w;
  }

  // Runs Real-ESRGAN on a folder, without a console window. Output goes to realesrgan.log next to the work folder.
  static std::string runTool(const fs::path& toolDir, const fs::path& logFile, const fs::path& in, const fs::path& out,
                             const std::function<bool()>& poll)
  {
    std::wstring cmd = L"\"" + (toolDir / kExe).wstring() + L"\" -i \"" + in.wstring() + L"\" -o \"" + out.wstring() +
                       L"\" -n realesrgan-x4plus -s 4 -f png -m \"" + (toolDir / L"models").wstring() + L"\"";
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE log = CreateFileW(logFile.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    if (log != INVALID_HANDLE_VALUE)
    {
      si.dwFlags = STARTF_USESTDHANDLES;
      si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
      si.hStdOutput = log;
      si.hStdError = log;
    }
    PROCESS_INFORMATION pi{};
    std::wstring dir = toolDir.wstring();
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, log != INVALID_HANDLE_VALUE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
                             nullptr, dir.c_str(), &si, &pi);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (!ok) return "cannot start realesrgan-ncnn-vulkan.exe (error " + std::to_string(GetLastError()) + ")";
    CloseHandle(pi.hThread);
    bool stopped = false;
    while (WaitForSingleObject(pi.hProcess, 500) == WAIT_TIMEOUT)
      if (!poll()) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); stopped = true; break; }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    poll();
    if (stopped) return "cancelled";
    if (code != 0)
      return "Real-ESRGAN failed (exit code " + std::to_string(code) + "). A graphics driver with Vulkan support is needed; details in " +
             logFile.filename().string();
    return "";
  }

  std::wstring generate(const fs::path& toolDir, const fs::path& gameDir, Progress& progress)
  {
    if (!toolAvailable(toolDir)) return L"Real-ESRGAN not found in " + toolDir.wstring();
    const fs::path cache = hdCore::cacheDir(gameDir);
    const fs::path work = cache / L"work";
    const fs::path logFile = cache / L"realesrgan.log";
    std::error_code ec;
    fs::create_directories(cache, ec);
    fs::remove(logFile, ec);

    hdCore::Progress p;
    p.report = [&](int stage, int done, int total) {
      progress.stage = stage == 0 ? StageReading : stage == 1 ? StageUpscaling : StagePacking;
      progress.done = done;
      progress.total = total;
    };
    p.cancelled = [&] { return progress.cancel.load(); };
    hdCore::Runner run = [&](const fs::path& in, const fs::path& out, const std::function<bool()>& poll) {
      return runTool(toolDir, logFile, in, out, poll);
    };
    unsigned cores = std::thread::hardware_concurrency();
    int threads = int(std::max(1u, std::min(4u, cores ? cores : 1u))); // 32-bit launcher: keep memory low
    std::string err;
    try
    {
      err = hdCore::generate(gameDir, work, run, p, 48, threads);
    }
    catch (const std::exception& e)
    {
      err = std::string("unexpected error: ") + e.what();
      fs::remove_all(work, ec);
    }
    progress.stage = StageDone;
    return widen(err);
  }

  std::wstring apply(const fs::path& gameDir, bool enabled)
  {
    try
    {
      return widen(hdCore::apply(gameDir, enabled));
    }
    catch (const std::exception& e)
    {
      return widen(e.what());
    }
  }
}
