#pragma once

#include <filesystem>
#include <memory>

#include "include/cef_app.h"

namespace mockterm::platform {

class BrowserBridge;

struct HostOptions {
  std::filesystem::path apps_dir;  // directory served as mockterm://apps/
  std::string start_url;           // initial URL (defaults to the shell)
};

// CefApp shared by all processes. The browser process creates the market-data
// bridge and the main window; renderer processes install window.mockterm.
class MockTermApp : public CefApp, public CefBrowserProcessHandler {
 public:
  explicit MockTermApp(HostOptions options);
  ~MockTermApp() override;

  // CefApp
  void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;

  // CefBrowserProcessHandler
  void OnContextInitialized() override;

  void Shutdown();

 private:
  HostOptions options_;
  std::shared_ptr<BrowserBridge> bridge_;
  CefRefPtr<CefRenderProcessHandler> renderer_;
  IMPLEMENT_REFCOUNTING(MockTermApp);
};

}  // namespace mockterm::platform
