#include <windows.h>
#include <shobjidl.h>

#include <cstdio>

int wmain() {
  const CLSID clsid = {0x7f07b25c, 0x22de, 0x46d3, {0x97, 0x47, 0x6b, 0x2d, 0x6b, 0x07, 0xf5, 0x4d}};
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    wprintf(L"CoInitializeEx failed: 0x%08X\n", hr);
    return 1;
  }

  IExplorerCommand* command = nullptr;
  hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&command));
  if (FAILED(hr)) {
    wprintf(L"CoCreateInstance failed: 0x%08X\n", hr);
    CoUninitialize();
    return 2;
  }

  PWSTR title = nullptr;
  hr = command->GetTitle(nullptr, &title);
  if (SUCCEEDED(hr)) {
    wprintf(L"title=%ls\n", title);
    CoTaskMemFree(title);
  } else {
    wprintf(L"GetTitle failed: 0x%08X\n", hr);
  }

  command->Release();
  CoUninitialize();
  return FAILED(hr) ? 3 : 0;
}
