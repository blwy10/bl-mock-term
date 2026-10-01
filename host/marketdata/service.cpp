#include "service.h"

#include <chrono>

#include "history.h"
#include "instruments.h"

namespace mockterm::md {

namespace {
uint64_t ResolveSeed(uint64_t seed) {
  return seed ? seed : static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}
}  // namespace

int64_t MarketDataService::NowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

MarketDataService::MarketDataService(MarketDataListener* listener, ServiceOptions options)
    : listener_(listener),
      options_(options),
      subs_(options.throttle_ms),
      sim_(DefaultInstruments(), ResolveSeed(options.seed)),
      news_(ResolveSeed(options.seed) ^ 0x9e3779b97f4a7c15ULL),
      chat_(ResolveSeed(options.seed) ^ 0x5851f42d4c957f2dULL),
      rng_(ResolveSeed(options.seed) ^ 0x2545f4914f6cdd1dULL) {
  int64_t now = NowMs();
  // Backfill the archive with recent, back-dated headlines.
  for (int i = options_.news_backfill; i > 0; --i) news_.Generate(sim_, now - i * 97'000);
  next_news_ms_ = now + options_.news_min_ms;
}

MarketDataService::~MarketDataService() { Stop(); }

void MarketDataService::Start() {
  if (running_.exchange(true)) return;
  thread_ = std::thread([this] { Run(); });
}

void MarketDataService::Stop() {
  if (!running_.exchange(false)) return;
  wake_.notify_all();
  if (thread_.joinable()) thread_.join();
}

void MarketDataService::Run() {
  std::uniform_int_distribution<int64_t> news_gap(options_.news_min_ms, options_.news_max_ms);
  while (running_) {
    int64_t now = NowMs();
    std::vector<Headline> headlines;
    std::vector<ChatMessage> replies;
    {
      std::lock_guard<std::mutex> lock(mu_);
      for (const Tick& t : sim_.Step(now, options_.tick_probability)) subs_.OnTick(t);
      if (now >= next_news_ms_) {
        headlines.push_back(news_.Generate(sim_, now));
        next_news_ms_ = now + news_gap(rng_);
      }
      replies = chat_.Poll(sim_, now);
    }
    if (listener_) {
      for (Delivery& d : subs_.Collect(now)) listener_->OnTicks(d.subscriber, std::move(d.ticks));
      for (Headline& h : headlines) listener_->OnNews(std::move(h));
      for (ChatMessage& m : replies) listener_->OnChat(std::move(m));
    }
    std::unique_lock<std::mutex> lock(wake_mu_);
    wake_.wait_for(lock, std::chrono::milliseconds(options_.loop_interval_ms), [this] { return !running_; });
  }
}

std::vector<std::string> MarketDataService::Subscribe(SubscriberId id, const std::vector<std::string>& tickers) {
  std::vector<std::string> known;
  std::vector<Tick> snapshots;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& t : tickers) {
      if (auto snap = sim_.Snapshot(t)) {
        known.push_back(t);
        snapshots.push_back(*snap);
      }
    }
  }
  subs_.Subscribe(id, known);
  for (const Tick& s : snapshots) subs_.Seed(id, s);
  return known;
}

void MarketDataService::Unsubscribe(SubscriberId id) { subs_.RemoveSubscriber(id); }

std::vector<Instrument> MarketDataService::Instruments() const {
  std::lock_guard<std::mutex> lock(mu_);
  return sim_.instruments();
}

std::optional<Instrument> MarketDataService::FindInstrument(const std::string& ticker) const {
  std::lock_guard<std::mutex> lock(mu_);
  const Instrument* i = sim_.Find(ticker);
  return i ? std::optional<Instrument>(*i) : std::nullopt;
}

std::optional<Tick> MarketDataService::Snapshot(const std::string& ticker) const {
  std::lock_guard<std::mutex> lock(mu_);
  return sim_.Snapshot(ticker);
}

std::optional<std::vector<Bar>> MarketDataService::History(const std::string& ticker, HistoryRange range) const {
  std::lock_guard<std::mutex> lock(mu_);
  const Instrument* inst = sim_.Find(ticker);
  auto snap = sim_.Snapshot(ticker);
  if (!inst || !snap) return std::nullopt;
  return GenerateHistory(*inst, range, snap->last, NowMs());
}

std::vector<Headline> MarketDataService::News(const std::string& query) const {
  std::lock_guard<std::mutex> lock(mu_);
  return news_.Search(query);
}

ChatMessage MarketDataService::SendChat(const std::string& to, const std::string& text) {
  std::lock_guard<std::mutex> lock(mu_);
  return chat_.Send(to, text, NowMs());
}

}  // namespace mockterm::md
