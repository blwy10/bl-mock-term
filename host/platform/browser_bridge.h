#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "include/cef_browser.h"
#include "include/cef_process_message.h"
#include "service.h"

namespace mockterm::platform {

// Browser-process end of the JS bridge: translates renderer process messages
// into MarketDataService calls and routes streaming data back to frames.
// All public methods except the MarketDataListener callbacks run on the UI
// thread.
class BrowserBridge : public md::MarketDataListener,
                      public std::enable_shared_from_this<BrowserBridge> {
 public:
  BrowserBridge();
  ~BrowserBridge() override;

  void Start();
  void Shutdown();

  void OnBrowserCreated(CefRefPtr<CefBrowser> browser);
  void OnBrowserClosed(CefRefPtr<CefBrowser> browser);
  void OnFrameDetached(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame);
  bool OnProcessMessage(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefProcessMessage> message);

  // md::MarketDataListener (service thread).
  void OnTicks(md::SubscriberId subscriber, std::vector<md::Tick> ticks) override;
  void OnNews(md::Headline headline) override;
  void OnChat(md::ChatMessage message) override;

 private:
  using FrameKey = std::pair<int, std::string>;  // browser id, frame id
  struct Subscription {
    FrameKey frame;
    int js_id = 0;
  };

  void HandleRequest(CefRefPtr<CefFrame> frame, CefRefPtr<CefListValue> args);
  void Respond(CefRefPtr<CefFrame> frame, int req_id, bool ok, const std::string& payload);
  void DeliverTicks(md::SubscriberId subscriber, const std::string& json);
  void Broadcast(std::set<FrameKey>& listeners, const char* name, const std::string& json);
  void DropFrame(const FrameKey& key);
  CefRefPtr<CefFrame> FindFrame(const FrameKey& key);

  std::unique_ptr<md::MarketDataService> service_;
  std::map<int, CefRefPtr<CefBrowser>> browsers_;
  std::map<md::SubscriberId, Subscription> subs_;
  std::map<std::pair<FrameKey, int>, md::SubscriberId> subs_by_js_;
  md::SubscriberId next_subscriber_ = 1;
  std::set<FrameKey> news_listeners_;
  std::set<FrameKey> chat_listeners_;
};

}  // namespace mockterm::platform
