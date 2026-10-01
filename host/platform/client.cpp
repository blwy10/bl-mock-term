#include "platform/client.h"

#include <windows.h>

#include "include/cef_app.h"

namespace mockterm::platform {

MockTermClient::MockTermClient(std::shared_ptr<BrowserBridge> bridge) : bridge_(std::move(bridge)) {}

bool MockTermClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                              CefProcessId, CefRefPtr<CefProcessMessage> message) {
  return bridge_->OnProcessMessage(browser, frame, message);
}

void MockTermClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
  CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
  if (hwnd) SetWindowTextW(hwnd, std::wstring(title).c_str());
}

void MockTermClient::OnFrameDetached(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
  bridge_->OnFrameDetached(browser, frame);
}

bool MockTermClient::OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int, const CefString&,
                                   const CefString&, CefLifeSpanHandler::WindowOpenDisposition, bool,
                                   const CefPopupFeatures&, CefWindowInfo&, CefRefPtr<CefClient>&,
                                   CefBrowserSettings&, CefRefPtr<CefDictionaryValue>&, bool*) {
  return true;  // No popups; apps open as panels via mockterm.openApp().
}

void MockTermClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  ++browser_count_;
  bridge_->OnBrowserCreated(browser);
}

void MockTermClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  bridge_->OnBrowserClosed(browser);
  if (--browser_count_ == 0) CefQuitMessageLoop();
}

}  // namespace mockterm::platform
