#pragma once

#include <windows.h>

#include <string>

namespace codex {

std::wstring BuildCodexOpenProjectUri(const std::wstring& path);

HRESULT LaunchCodexProtocolUri(const std::wstring& uri);

HRESULT LaunchCodexForPath(const std::wstring& path);

}  // namespace codex
