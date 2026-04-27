#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>

#include <new>
#include <string>
#include <vector>

namespace {

// {7F07B25C-22DE-46D3-9747-6B2D6B07F54D}
constexpr CLSID CLSID_CodexExplorerCommand =
    {0x7f07b25c, 0x22de, 0x46d3, {0x97, 0x47, 0x6b, 0x2d, 0x6b, 0x07, 0xf5, 0x4d}};

long g_moduleRefCount = 0;

#define CODEX_RETURN_IF_FAILED(expr) \
  do { \
    const HRESULT _codex_hr = (expr); \
    if (FAILED(_codex_hr)) { \
      return _codex_hr; \
    } \
  } while (false)

class ModuleRef {
 public:
  ModuleRef() { InterlockedIncrement(&g_moduleRefCount); }
  ~ModuleRef() { InterlockedDecrement(&g_moduleRefCount); }
};

PWSTR AllocShellString(const wchar_t* value) {
  if (value == nullptr) {
    return nullptr;
  }
  const size_t len = wcslen(value);
  auto* result = static_cast<PWSTR>(CoTaskMemAlloc((len + 1) * sizeof(wchar_t)));
  if (result == nullptr) {
    return nullptr;
  }
  memcpy(result, value, (len + 1) * sizeof(wchar_t));
  return result;
}

std::wstring QueryDefaultValue(HKEY root, const wchar_t* subkey) {
  DWORD type = 0;
  DWORD cb = 0;
  if (RegGetValueW(root, subkey, nullptr, RRF_RT_REG_SZ, &type, nullptr, &cb) != ERROR_SUCCESS || cb == 0) {
    return L"";
  }

  std::vector<wchar_t> buffer(cb / sizeof(wchar_t) + 1);
  if (RegGetValueW(root, subkey, nullptr, RRF_RT_REG_SZ, &type, buffer.data(), &cb) != ERROR_SUCCESS) {
    return L"";
  }
  return std::wstring(buffer.data());
}

void ReplaceAll(std::wstring& text, const std::wstring& needle, const std::wstring& replacement) {
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::wstring::npos) {
    text.replace(pos, needle.length(), replacement);
    pos += replacement.length();
  }
}

std::wstring ReadCodexCommandTemplate() {
  std::wstring value = QueryDefaultValue(
      HKEY_CURRENT_USER,
      L"Software\\Classes\\Directory\\Background\\shell\\OpenProjectInCodex\\command");
  if (!value.empty()) {
    return value;
  }
  return QueryDefaultValue(
      HKEY_CURRENT_USER,
      L"Software\\Classes\\Directory\\shell\\OpenProjectInCodex\\command");
}

std::wstring ReadCodexIconPath() {
  DWORD type = 0;
  DWORD cb = 0;
  const wchar_t* subkey = L"Software\\Classes\\Directory\\Background\\shell\\OpenProjectInCodex";
  if (RegGetValueW(HKEY_CURRENT_USER, subkey, L"Icon", RRF_RT_REG_SZ, &type, nullptr, &cb) != ERROR_SUCCESS || cb == 0) {
    return L"";
  }

  std::vector<wchar_t> buffer(cb / sizeof(wchar_t) + 1);
  if (RegGetValueW(HKEY_CURRENT_USER, subkey, L"Icon", RRF_RT_REG_SZ, &type, buffer.data(), &cb) != ERROR_SUCCESS) {
    return L"";
  }
  std::wstring value(buffer.data());
  if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
    value = value.substr(1, value.size() - 2);
  }
  return value;
}

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

  PWSTR filePath = nullptr;
  HRESULT hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
  item->Release();
  if (FAILED(hr)) {
    return hr;
  }

  path.assign(filePath);
  CoTaskMemFree(filePath);
  return path.empty() ? E_FAIL : S_OK;
}

HRESULT LaunchCodexForPath(const std::wstring& path) {
  std::wstring command = ReadCodexCommandTemplate();
  if (command.empty()) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  ReplaceAll(command, L"%V", path);
  ReplaceAll(command, L"%1", path);

  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};

  std::vector<wchar_t> mutableCommand(command.begin(), command.end());
  mutableCommand.push_back(L'\0');

  if (!CreateProcessW(
          nullptr,
          mutableCommand.data(),
          nullptr,
          nullptr,
          FALSE,
          0,
          nullptr,
          path.c_str(),
          &startup,
          &process)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return S_OK;
}

class CodexExplorerCommand final : public IExplorerCommand {
 public:
  CodexExplorerCommand() : refCount_(1) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_POINTER;
    }
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IExplorerCommand)) {
      *ppv = static_cast<IExplorerCommand*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&refCount_);
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    const ULONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
      delete this;
    }
    return count;
  }

  IFACEMETHODIMP GetTitle(IShellItemArray*, PWSTR* name) override {
    if (name == nullptr) {
      return E_POINTER;
    }
    *name = AllocShellString(L"Open project in Codex");
    return *name == nullptr ? E_OUTOFMEMORY : S_OK;
  }

  IFACEMETHODIMP GetIcon(IShellItemArray*, PWSTR* icon) override {
    if (icon == nullptr) {
      return E_POINTER;
    }
    const std::wstring iconPath = ReadCodexIconPath();
    *icon = AllocShellString(iconPath.empty() ? L"" : iconPath.c_str());
    return *icon == nullptr ? E_OUTOFMEMORY : S_OK;
  }

  IFACEMETHODIMP GetToolTip(IShellItemArray*, PWSTR* tip) override {
    if (tip == nullptr) {
      return E_POINTER;
    }
    *tip = AllocShellString(L"Open this folder as a Codex project");
    return *tip == nullptr ? E_OUTOFMEMORY : S_OK;
  }

  IFACEMETHODIMP GetCanonicalName(GUID* guidCommandName) override {
    if (guidCommandName == nullptr) {
      return E_POINTER;
    }
    *guidCommandName = CLSID_CodexExplorerCommand;
    return S_OK;
  }

  IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* state) override {
    if (state == nullptr) {
      return E_POINTER;
    }
    *state = ECS_ENABLED;
    return S_OK;
  }

  IFACEMETHODIMP Invoke(IShellItemArray* selection, IBindCtx*) override {
    std::wstring path;
    CODEX_RETURN_IF_FAILED(GetPathFromSelection(selection, path));
    return LaunchCodexForPath(path);
  }

  IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override {
    if (flags == nullptr) {
      return E_POINTER;
    }
    *flags = ECF_DEFAULT;
    return S_OK;
  }

  IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** enumCommands) override {
    if (enumCommands != nullptr) {
      *enumCommands = nullptr;
    }
    return E_NOTIMPL;
  }

 private:
  ~CodexExplorerCommand() = default;

  ModuleRef moduleRef_;
  long refCount_;
};

class ClassFactory final : public IClassFactory {
 public:
  ClassFactory() : refCount_(1) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_POINTER;
    }
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
      *ppv = static_cast<IClassFactory*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&refCount_);
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    const ULONG count = InterlockedDecrement(&refCount_);
    if (count == 0) {
      delete this;
    }
    return count;
  }

  IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
    if (outer != nullptr) {
      return CLASS_E_NOAGGREGATION;
    }
    auto* command = new (std::nothrow) CodexExplorerCommand();
    if (command == nullptr) {
      return E_OUTOFMEMORY;
    }
    const HRESULT hr = command->QueryInterface(riid, ppv);
    command->Release();
    return hr;
  }

  IFACEMETHODIMP LockServer(BOOL lock) override {
    if (lock) {
      InterlockedIncrement(&g_moduleRefCount);
    } else {
      InterlockedDecrement(&g_moduleRefCount);
    }
    return S_OK;
  }

 private:
  ~ClassFactory() = default;

  ModuleRef moduleRef_;
  long refCount_;
};

}  // namespace

STDAPI DllCanUnloadNow() {
  return g_moduleRefCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
  if (!IsEqualCLSID(clsid, CLSID_CodexExplorerCommand)) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }

  auto* factory = new (std::nothrow) ClassFactory();
  if (factory == nullptr) {
    return E_OUTOFMEMORY;
  }
  const HRESULT hr = factory->QueryInterface(riid, ppv);
  factory->Release();
  return hr;
}
