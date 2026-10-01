#include "platform/app.h"

#include <windows.h>

#include "include/cef_browser.h"
#include "platform/browser_bridge.h"
#include "platform/client.h"
#include "platform/ipc.h"
#include "platform/renderer_bridge.h"
#include "platform/scheme.h"

namespace mockterm::platform {

MockTermApp::MockTermApp(HostOptions options) : options_(std::move(options)) {}

MockTermApp::~MockTermApp() = default;

void MockTermApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  RegisterCustomSchemes(registrar);
}

CefRefPtr<CefRenderProcessHandler> MockTermApp::GetRenderProcessHandler() {
  if (!renderer_) renderer_ = new RendererBridge();
  return renderer_;
}

void MockTermApp::OnContextInitialized() {
  RegisterAppSchemeHandler(options_.apps_dir);

  md::CommsOptions comms;
  comms.port = options_.comms_port;
  comms.spawn = options_.spawn_comms;
  comms.comms_args = options_.comms_args;
  if (!options_.log_dir.empty()) comms.log_file = options_.log_dir / L"mtclient.log";
  bridge_ = std::make_shared<BrowserBridge>(std::move(comms));
  bridge_->Start();

  CefWindowInfo window_info;
  window_info.SetAsPopup(nullptr, "MockTerm");
  window_info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  window_info.bounds.x = 40;
  window_info.bounds.y = 40;
  window_info.bounds.width = 1600;
  window_info.bounds.height = 980;

  CefBrowserSettings browser_settings;
  browser_settings.background_color = CefColorSetARGB(255, 0, 0, 0);

  const std::string url = options_.start_url.empty() ? ipc::kShellUrl : options_.start_url;
  CefBrowserHost::CreateBrowser(window_info, new MockTermClient(bridge_), url, browser_settings,
                                nullptr, nullptr);
}

void MockTermApp::Shutdown() {
  if (bridge_) bridge_->Shutdown();
  bridge_.reset();
}

}  // namespace mockterm::platform
