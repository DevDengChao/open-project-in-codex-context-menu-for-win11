#include <windows.h>

#include <string>

#include "..\ShellExtension\CodexProtocolLauncher.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return static_cast<int>(HRESULT_FROM_WIN32(GetLastError()));
  }

  int exitCode = 2;
  if (argc >= 2 && argv[1] != nullptr && argv[1][0] != L'\0') {
    const HRESULT hr = codex::LaunchCodexForPath(argv[1]);
    exitCode = SUCCEEDED(hr) ? 0 : static_cast<int>(hr);
  }

  LocalFree(argv);
  return exitCode;
}
