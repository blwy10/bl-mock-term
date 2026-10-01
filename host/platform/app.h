#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "include/cef_app.h"

namespace mockterm::platform {

class BrowserBridge;

struct HostOptions {
  std::filesystem::path apps_dir;  // directory served as mockterm://apps/
  std::string start_url;           // initial URL (defaults to the shell)
  int comms_port = 8194;           // local mtcomm endpoint
  bool spawn_comms = true;         // start mtcomm.exe if it is not running
  std::vector<std::string> comms_args;  // extra mtcomm.exe arguments
  std::filesystem::path log_dir;   // where mtclient.log is written
};

// CefApp shared by all processes. The browser process creates the mtcomm-backed
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
