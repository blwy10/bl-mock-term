#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "include/cef_render_process_handler.h"

namespace mockterm::platform {

struct FrameBridgeState;

// Renderer-process end of the JS bridge: installs window.mockterm into every
// mockterm:// frame and forwards calls to the browser process.
class RendererBridge : public CefRenderProcessHandler {
 public:
  RendererBridge();

  void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;
  void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override;
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

 private:
  std::map<std::string, std::vector<std::shared_ptr<FrameBridgeState>>> frames_;
  IMPLEMENT_REFCOUNTING(RendererBridge);
};

}  // namespace mockterm::platform
