#pragma once

#include <windows.h>
#include <shobjidl.h>

#include <string>

namespace codex {

HRESULT GetPathFromSelection(IShellItemArray* selection, std::wstring& path);
HRESULT GetPathFromSite(IUnknown* site, std::wstring& path);
HRESULT ResolveCodexTargetPath(IShellItemArray* selection, IUnknown* site, std::wstring& path);

}  // namespace codex
