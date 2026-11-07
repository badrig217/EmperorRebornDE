#pragma once
// Creates "Emperor Spice.lnk" on the user's desktop, pointing at the launcher.
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")

inline std::wstring desktopShortcutPath()
{
  PWSTR desktop = nullptr;
  std::wstring path;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop)))
    path = std::wstring(desktop) + L"\\Emperor Spice.lnk";
  CoTaskMemFree(desktop);
  return path;
}

// Returns true if the shortcut exists afterwards.
inline bool createDesktopShortcut()
{
  wchar_t exePath[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  std::wstring workDir = exePath;
  workDir.resize(workDir.find_last_of(L"\\/"));

  std::wstring linkPath = desktopShortcutPath();
  if (linkPath.empty())
    return false;

  HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  bool ok = false;

  IShellLinkW* link = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&link))))
  {
    link->SetPath(exePath);
    link->SetWorkingDirectory(workDir.c_str());
    link->SetIconLocation(exePath, 0);
    link->SetDescription(L"Emperor Spice - Emperor: Battle for Dune");

    IPersistFile* file = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file))))
    {
      ok = SUCCEEDED(file->Save(linkPath.c_str(), TRUE));
      file->Release();
    }
    link->Release();
  }

  if (SUCCEEDED(init))
    CoUninitialize();
  return ok;
}
