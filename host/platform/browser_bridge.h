#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "comms_client.h"
#include "include/cef_browser.h"
#include "include/cef_process_message.h"

namespace mockterm::platform {

// Browser-process end of the JS bridge: translates renderer process messages
// into CommsClient calls (and so into mtcomm protocol traffic) and routes
// streaming data, responses and connection state back to frames.
// All public methods except the CommsListener callbacks run on the UI thread.
class BrowserBridge : public md::CommsListener,
                      public std::enable_shared_from_this<BrowserBridge> {
 public:
  explicit BrowserBridge(md::CommsOptions options);
  ~BrowserBridge() override;

  void Start();
  void Shutdown();

  void OnBrowserCreated(CefRefPtr<CefBrowser> browser);
  void OnBrowserClosed(CefRefPtr<CefBrowser> browser);
  void OnFrameDetached(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame);
  void OnMainFrameLoaded(CefRefPtr<CefBrowser> browser);
  bool OnProcessMessage(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefProcessMessage> message);

  // md::CommsListener (mtclient session thread).
  void OnTicks(md::SubscriberId subscriber, std::vector<md::Tick> ticks) override;
  void OnNews(md::Headline headline) override;
  void OnChat(md::ChatMessage message) override;
  void OnConnectionState(md::ConnectionState state) override;

 private:
  using FrameKey = std::pair<int, std::string>;  // browser id, frame id
  struct Subscription {
    FrameKey frame;
    int js_id = 0;
  };

  void HandleRequest(const FrameKey& key, CefRefPtr<CefListValue> args);
  md::Reply ReplyTo(const FrameKey& key, int req_id);
  void Respond(const FrameKey& key, int req_id, bool ok, const std::string& payload);
  void DeliverTicks(md::SubscriberId subscriber, const std::string& json);
  void Broadcast(std::set<FrameKey>& listeners, const char* name, const std::string& json);
  void DispatchStatus(CefRefPtr<CefBrowser> browser);
  void DropFrame(const FrameKey& key);
  CefRefPtr<CefFrame> FindFrame(const FrameKey& key);

  std::unique_ptr<md::CommsClient> client_;
  md::ConnectionState state_ = md::ConnectionState::Connecting;
  std::map<int, CefRefPtr<CefBrowser>> browsers_;
  std::map<md::SubscriberId, Subscription> subs_;
  std::map<std::pair<FrameKey, int>, md::SubscriberId> subs_by_js_;
  md::SubscriberId next_subscriber_ = 1;
  std::set<FrameKey> news_listeners_;
  std::set<FrameKey> chat_listeners_;
};

}  // namespace mockterm::platform
