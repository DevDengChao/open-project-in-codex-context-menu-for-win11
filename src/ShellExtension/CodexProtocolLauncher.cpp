#include "CodexProtocolLauncher.h"

#include <shellapi.h>

#include <string>

namespace {

bool IsUnreservedUriByte(unsigned char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
         value == '-' || value == '.' || value == '_' || value == '~';
}

std::wstring PercentEncodeUtf8(const std::wstring& value) {
  if (value.empty()) {
    return L"";
  }

  const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (utf8Length <= 1) {
    return L"";
  }

  std::string utf8(static_cast<size_t>(utf8Length - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &utf8[0], utf8Length - 1, nullptr, nullptr);

  static constexpr wchar_t kHexDigits[] = L"0123456789ABCDEF";

  std::wstring encoded;
  encoded.reserve(utf8.size() * 3);
  for (unsigned char ch : utf8) {
    if (IsUnreservedUriByte(ch)) {
      encoded.push_back(static_cast<wchar_t>(ch));
      continue;
    }

    encoded.push_back(L'%');
    encoded.push_back(kHexDigits[(ch >> 4) & 0x0F]);
    encoded.push_back(kHexDigits[ch & 0x0F]);
  }

  return encoded;
}

}  // namespace

namespace codex {

std::wstring BuildCodexOpenProjectUri(const std::wstring& path) {
  return L"codex://new?path=" + PercentEncodeUtf8(path);
}

HRESULT LaunchCodexProtocolUri(const std::wstring& uri) {
  SHELLEXECUTEINFOW executeInfo = {};
  executeInfo.cbSize = sizeof(executeInfo);
  executeInfo.fMask = SEE_MASK_NOASYNC;
  executeInfo.nShow = SW_SHOWNORMAL;
  executeInfo.lpVerb = L"open";
  executeInfo.lpFile = uri.c_str();

  if (!ShellExecuteExW(&executeInfo)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  return S_OK;
}

HRESULT LaunchCodexForPath(const std::wstring& path) {
  return LaunchCodexProtocolUri(BuildCodexOpenProjectUri(path));
}

}  // namespace codex
