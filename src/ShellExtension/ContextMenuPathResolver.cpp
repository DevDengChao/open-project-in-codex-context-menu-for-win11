#include "ContextMenuPathResolver.h"

#include <shlobj.h>
#include <servprov.h>
#include <string>
#include <vector>

namespace {

#define CODEX_RETURN_IF_FAILED(expr) \
  do { \
    const HRESULT _codex_hr = (expr); \
    if (FAILED(_codex_hr)) { \
      return _codex_hr; \
    } \
  } while (false)

HRESULT GetDisplayNameFromShellItem(IShellItem* item, std::wstring& path) {
  if (item == nullptr) {
    return E_INVALIDARG;
  }

  PWSTR filePath = nullptr;
  const HRESULT hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
  if (FAILED(hr)) {
    return hr;
  }

  path.assign(filePath == nullptr ? L"" : filePath);
  CoTaskMemFree(filePath);
  return path.empty() ? E_FAIL : S_OK;
}

HRESULT GetPathFromFolderView(IFolderView* folderView, std::wstring& path) {
  if (folderView == nullptr) {
    return E_INVALIDARG;
  }

  IPersistFolder2* persistFolder = nullptr;
  CODEX_RETURN_IF_FAILED(folderView->GetFolder(IID_PPV_ARGS(&persistFolder)));

  PIDLIST_ABSOLUTE pidl = nullptr;
  const HRESULT hr = persistFolder->GetCurFolder(&pidl);
  persistFolder->Release();
  if (FAILED(hr)) {
    return hr;
  }

  wchar_t buffer[MAX_PATH];
  if (!SHGetPathFromIDListW(pidl, buffer)) {
    CoTaskMemFree(pidl);
    return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  }

  CoTaskMemFree(pidl);
  path.assign(buffer);
  return path.empty() ? E_FAIL : S_OK;
}

}  // namespace

namespace codex {

HRESULT GetPathFromSelection(IShellItemArray* selection, std::wstring& path) {
  if (selection == nullptr) {
    return E_INVALIDARG;
  }

  DWORD count = 0;
  CODEX_RETURN_IF_FAILED(selection->GetCount(&count));
  if (count == 0) {
    return E_FAIL;
  }

  IShellItem* item = nullptr;
  CODEX_RETURN_IF_FAILED(selection->GetItemAt(0, &item));
  const HRESULT hr = GetDisplayNameFromShellItem(item, path);
  item->Release();
  return hr;
}

HRESULT GetPathFromSite(IUnknown* site, std::wstring& path) {
  if (site == nullptr) {
    return E_INVALIDARG;
  }

  IServiceProvider* serviceProvider = nullptr;
  CODEX_RETURN_IF_FAILED(site->QueryInterface(IID_PPV_ARGS(&serviceProvider)));

  IFolderView* folderView = nullptr;
  const HRESULT hr = serviceProvider->QueryService(SID_SFolderView, IID_PPV_ARGS(&folderView));
  serviceProvider->Release();
  if (FAILED(hr)) {
    return hr;
  }

  const HRESULT pathHr = GetPathFromFolderView(folderView, path);
  folderView->Release();
  return pathHr;
}

HRESULT ResolveCodexTargetPath(IShellItemArray* selection, IUnknown* site, std::wstring& path) {
  if (selection != nullptr) {
    const HRESULT selectionHr = GetPathFromSelection(selection, path);
    if (SUCCEEDED(selectionHr)) {
      return S_OK;
    }
  }

  return GetPathFromSite(site, path);
}

}  // namespace codex
