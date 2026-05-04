#include <windows.h>
#include <ocidl.h>
#include <shobjidl.h>
#include <shlobj.h>

#include <string>

#include "..\..\src\ShellExtension\CodexProtocolLauncher.h"
#include "..\..\src\ShellExtension\ContextMenuPathResolver.h"

#include <cstdio>

namespace {

class FakeShellItemArray;

class RefCounted {
 public:
  ULONG AddRef() {
    return InterlockedIncrement(&ref_count_);
  }

  ULONG Release() {
    const ULONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) {
      delete this;
    }
    return count;
  }

 protected:
  virtual ~RefCounted() = default;

 private:
  long ref_count_ = 1;
};

class FakeShellItem final : public IShellItem, public RefCounted {
 public:
  explicit FakeShellItem(std::wstring path) : path_(std::move(path)) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_POINTER;
    }
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IShellItem)) {
      *ppv = static_cast<IShellItem*>(this);
      RefCounted::AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override { return RefCounted::AddRef(); }
  IFACEMETHODIMP_(ULONG) Release() override { return RefCounted::Release(); }

  IFACEMETHODIMP BindToHandler(IBindCtx*, REFGUID, REFIID, void**) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetParent(IShellItem**) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetDisplayName(SIGDN sigdnName, PWSTR* ppszName) override {
    if (ppszName == nullptr) {
      return E_POINTER;
    }
    *ppszName = nullptr;
    if (sigdnName != SIGDN_FILESYSPATH) {
      return E_NOTIMPL;
    }
    const size_t bytes = (path_.size() + 1) * sizeof(wchar_t);
    auto* buffer = static_cast<PWSTR>(CoTaskMemAlloc(bytes));
    if (buffer == nullptr) {
      return E_OUTOFMEMORY;
    }
    memcpy(buffer, path_.c_str(), bytes);
    *ppszName = buffer;
    return S_OK;
  }
  IFACEMETHODIMP GetAttributes(SFGAOF, SFGAOF*) override { return E_NOTIMPL; }
  IFACEMETHODIMP Compare(IShellItem*, SICHINTF, int*) override { return E_NOTIMPL; }

 private:
  std::wstring path_;
};

class FakeShellItemArray final : public IShellItemArray, public RefCounted {
 public:
  explicit FakeShellItemArray(std::wstring path) : item_(new FakeShellItem(std::move(path))) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_POINTER;
    }
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IShellItemArray)) {
      *ppv = static_cast<IShellItemArray*>(this);
      RefCounted::AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override { return RefCounted::AddRef(); }
  IFACEMETHODIMP_(ULONG) Release() override { return RefCounted::Release(); }

  IFACEMETHODIMP BindToHandler(IBindCtx*, REFGUID, REFIID, void**) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetPropertyStore(GETPROPERTYSTOREFLAGS, REFIID, void**) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetPropertyDescriptionList(REFPROPERTYKEY, REFIID, void**) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetAttributes(SIATTRIBFLAGS, SFGAOF, SFGAOF*) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetCount(DWORD* pdwNumItems) override {
    if (pdwNumItems == nullptr) {
      return E_POINTER;
    }
    *pdwNumItems = 1;
    return S_OK;
  }
  IFACEMETHODIMP GetItemAt(DWORD dwIndex, IShellItem** ppsi) override {
    if (ppsi == nullptr) {
      return E_POINTER;
    }
    *ppsi = nullptr;
    if (dwIndex != 0) {
      return E_INVALIDARG;
    }
    item_->AddRef();
    *ppsi = item_;
    return S_OK;
  }
  IFACEMETHODIMP EnumItems(IEnumShellItems**) override { return E_NOTIMPL; }

 private:
  FakeShellItem* item_;
};

class FakePersistFolder2 final : public IPersistFolder2, public RefCounted {
 public:
  explicit FakePersistFolder2(std::wstring path) : path_(std::move(path)) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_POINTER;
    }
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IPersist) ||
        IsEqualIID(riid, IID_IPersistFolder) || IsEqualIID(riid, IID_IPersistFolder2)) {
      *ppv = static_cast<IPersistFolder2*>(this);
      RefCounted::AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override { return RefCounted::AddRef(); }
  IFACEMETHODIMP_(ULONG) Release() override { return RefCounted::Release(); }

  IFACEMETHODIMP GetClassID(CLSID*) override { return E_NOTIMPL; }
  IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetCurFolder(PIDLIST_ABSOLUTE* ppidl) override {
    if (ppidl == nullptr) {
      return E_POINTER;
    }
    *ppidl = nullptr;
    return SHParseDisplayName(path_.c_str(), nullptr, ppidl, 0, nullptr);
  }

 private:
  std::wstring path_;
};

class FakeFolderView final : public IFolderView, public RefCounted {
 public:
  explicit FakeFolderView(std::wstring path) : folder_(new FakePersistFolder2(std::move(path))) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_POINTER;
    }
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IFolderView)) {
      *ppv = static_cast<IFolderView*>(this);
      RefCounted::AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override { return RefCounted::AddRef(); }
  IFACEMETHODIMP_(ULONG) Release() override { return RefCounted::Release(); }

  IFACEMETHODIMP GetCurrentViewMode(UINT*) override { return E_NOTIMPL; }
  IFACEMETHODIMP SetCurrentViewMode(UINT) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetFolder(REFIID riid, void** ppv) override {
    return folder_->QueryInterface(riid, ppv);
  }
  IFACEMETHODIMP Item(int, PITEMID_CHILD*) override { return E_NOTIMPL; }
  IFACEMETHODIMP ItemCount(UINT, int*) override { return E_NOTIMPL; }
  IFACEMETHODIMP Items(UINT, REFIID, void**) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetSelectionMarkedItem(int*) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetFocusedItem(int*) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetItemPosition(PCUITEMID_CHILD, POINT*) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetSpacing(POINT*) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetDefaultSpacing(POINT*) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetAutoArrange() override { return E_NOTIMPL; }
  IFACEMETHODIMP SelectItem(int, DWORD) override { return E_NOTIMPL; }
  IFACEMETHODIMP SelectAndPositionItems(UINT, PCUITEMID_CHILD_ARRAY, POINT*, DWORD) override { return E_NOTIMPL; }

 private:
  FakePersistFolder2* folder_;
};

class FakeSite final : public IServiceProvider, public RefCounted {
 public:
  explicit FakeSite(std::wstring path) : folder_view_(new FakeFolderView(std::move(path))) {}

  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_POINTER;
    }
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IServiceProvider)) {
      *ppv = static_cast<IServiceProvider*>(this);
      RefCounted::AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override { return RefCounted::AddRef(); }
  IFACEMETHODIMP_(ULONG) Release() override { return RefCounted::Release(); }

  IFACEMETHODIMP QueryService(REFGUID guidService, REFIID riid, void** ppv) override {
    if (!IsEqualGUID(guidService, SID_SFolderView)) {
      return E_NOINTERFACE;
    }
    return folder_view_->QueryInterface(riid, ppv);
  }

 private:
  FakeFolderView* folder_view_;
};

bool TestResolvePathFromSelection() {
  std::wstring path;
  auto* selection = new FakeShellItemArray(L"D:\\workspace\\tmp\\bby");
  const HRESULT hr = codex::ResolveCodexTargetPath(selection, nullptr, path);
  selection->Release();
  if (FAILED(hr) || path != L"D:\\workspace\\tmp\\bby") {
    wprintf(L"ResolveCodexTargetPath(selection) failed: hr=0x%08X path=%ls\n", hr, path.c_str());
    return false;
  }
  return true;
}

bool TestResolvePathFromSite() {
  std::wstring path;
  auto* site = new FakeSite(L"D:\\workspace\\tmp\\bby");
  const HRESULT hr = codex::ResolveCodexTargetPath(nullptr, site, path);
  site->Release();
  if (FAILED(hr) || path != L"D:\\workspace\\tmp\\bby") {
    wprintf(L"ResolveCodexTargetPath(site) failed: hr=0x%08X path=%ls\n", hr, path.c_str());
    return false;
  }
  return true;
}

bool TestBuildCodexProtocolUri() {
  const std::wstring path = L"D:\\workspace\\tmp\\folder with spaces\\a#b.txt";
  const std::wstring uri = codex::BuildCodexOpenProjectUri(path);
  const std::wstring expected =
      L"codex://new?path=D%3A%5Cworkspace%5Ctmp%5Cfolder%20with%20spaces%5Ca%23b.txt";
  if (uri != expected) {
    wprintf(L"BuildCodexOpenProjectUri failed:\nexpected=%ls\nactual=%ls\n", expected.c_str(), uri.c_str());
    return false;
  }
  return true;
}

bool ShouldRunInvokeTest() {
  wchar_t buffer[8] = {};
  const DWORD length = GetEnvironmentVariableW(L"CODEX_COMSMOKE_INVOKE", buffer, ARRAYSIZE(buffer));
  return length > 0 && wcscmp(buffer, L"1") == 0;
}

}  // namespace

int wmain() {
  if (!TestResolvePathFromSelection()) {
    return 10;
  }

  if (!TestResolvePathFromSite()) {
    return 11;
  }

  if (!TestBuildCodexProtocolUri()) {
    return 12;
  }
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

  if (SUCCEEDED(hr) && ShouldRunInvokeTest()) {
    IObjectWithSite* objectWithSite = nullptr;
    hr = command->QueryInterface(IID_PPV_ARGS(&objectWithSite));
    if (SUCCEEDED(hr)) {
      auto* site = new FakeSite(L"D:\\workspace\\tmp\\bby");
      hr = objectWithSite->SetSite(site);
      site->Release();
      if (SUCCEEDED(hr)) {
        hr = command->Invoke(nullptr, nullptr);
        if (FAILED(hr)) {
          wprintf(L"Invoke failed: 0x%08X\n", hr);
        }
      } else {
        wprintf(L"SetSite failed: 0x%08X\n", hr);
      }
      objectWithSite->Release();
    } else {
      wprintf(L"QueryInterface(IObjectWithSite) failed: 0x%08X\n", hr);
    }
  }

  command->Release();
  CoUninitialize();
  return FAILED(hr) ? 3 : 0;
}
