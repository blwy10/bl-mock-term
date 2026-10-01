#include <gtest/gtest.h>

#include "subscription_manager.h"

using namespace mockterm::md;

namespace {
Tick T(const std::string& ticker, double last, uint64_t seq = 0) {
  Tick t;
  t.ticker = ticker;
  t.last = last;
  t.seq = seq;
  return t;
}
}  // namespace

TEST(SubscriptionManager, DeliversOnlySubscribedTickers) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"AAPL US Equity"});
  m.OnTick(T("AAPL US Equity", 1));
  m.OnTick(T("MSFT US Equity", 2));
  auto out = m.Collect(1000);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].subscriber, 1u);
  ASSERT_EQ(out[0].ticks.size(), 1u);
  EXPECT_EQ(out[0].ticks[0].ticker, "AAPL US Equity");
}

TEST(SubscriptionManager, CoalescesToLatestTickPerTicker) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A", "B"});
  m.OnTick(T("A", 1, 1));
  m.OnTick(T("A", 2, 2));
  m.OnTick(T("B", 5, 3));
  m.OnTick(T("A", 3, 4));
  auto out = m.Collect(0);
  ASSERT_EQ(out.size(), 1u);
  ASSERT_EQ(out[0].ticks.size(), 2u);
  EXPECT_EQ(out[0].ticks[0].ticker, "A");
  EXPECT_DOUBLE_EQ(out[0].ticks[0].last, 3);
  EXPECT_EQ(out[0].ticks[1].ticker, "B");
}

TEST(SubscriptionManager, ThrottlesToMinInterval) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A"});
  m.OnTick(T("A", 1));
  EXPECT_EQ(m.Collect(1000).size(), 1u);
  m.OnTick(T("A", 2));
  EXPECT_TRUE(m.Collect(1050).empty());  // within the 100 ms window
  m.OnTick(T("A", 3));
  auto out = m.Collect(1100);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_DOUBLE_EQ(out[0].ticks[0].last, 3);  // coalesced across the window
}

TEST(SubscriptionManager, TenHertzCapUnderHeavyTickFlow) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A"});
  int deliveries = 0;
  for (int64_t now = 0; now < 1000; now += 5) {  // 200 ticks/s for one second
    m.OnTick(T("A", static_cast<double>(now)));
    deliveries += static_cast<int>(m.Collect(now).size());
  }
  EXPECT_EQ(deliveries, 10);
}

TEST(SubscriptionManager, NoDeliveryWithoutPendingTicks) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A"});
  EXPECT_TRUE(m.Collect(1000).empty());
}

TEST(SubscriptionManager, ThrottleIsPerSubscriber) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A"});
  m.OnTick(T("A", 1));
  EXPECT_EQ(m.Collect(1000).size(), 1u);
  m.Subscribe(2, {"A"});
  m.OnTick(T("A", 2));
  auto out = m.Collect(1010);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].subscriber, 2u);
}

TEST(SubscriptionManager, UnsubscribeStopsDeliveryAndDropsPending) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A", "B"});
  m.OnTick(T("A", 1));
  m.OnTick(T("B", 1));
  m.Unsubscribe(1, {"A"});
  auto out = m.Collect(0);
  ASSERT_EQ(out.size(), 1u);
  ASSERT_EQ(out[0].ticks.size(), 1u);
  EXPECT_EQ(out[0].ticks[0].ticker, "B");
  EXPECT_EQ(m.ActiveTickers(), std::set<std::string>{"B"});
  m.Unsubscribe(1, {"B"});
  EXPECT_EQ(m.SubscriberCount(), 0u);
  EXPECT_TRUE(m.ActiveTickers().empty());
}

TEST(SubscriptionManager, RemoveSubscriberCleansUpTickerIndex) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A", "B"});
  m.Subscribe(2, {"B"});
  m.RemoveSubscriber(1);
  EXPECT_EQ(m.SubscriberCount(), 1u);
  EXPECT_EQ(m.ActiveTickers(), std::set<std::string>{"B"});
  m.OnTick(T("A", 1));
  m.OnTick(T("B", 1));
  auto out = m.Collect(0);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].subscriber, 2u);
  m.RemoveSubscriber(99);  // unknown id is a no-op
}

TEST(SubscriptionManager, SeedTargetsSingleSubscriber) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A"});
  m.Subscribe(2, {"A"});
  m.Seed(2, T("A", 42));
  m.Seed(2, T("Z", 1));  // not subscribed: ignored
  auto out = m.Collect(0);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].subscriber, 2u);
  ASSERT_EQ(out[0].ticks.size(), 1u);
  EXPECT_DOUBLE_EQ(out[0].ticks[0].last, 42);
}

TEST(SubscriptionManager, DuplicateSubscribeIsIdempotent) {
  SubscriptionManager m(100);
  m.Subscribe(1, {"A", "A"});
  m.Subscribe(1, {"A"});
  EXPECT_EQ(m.TickersFor(1), std::set<std::string>{"A"});
  m.OnTick(T("A", 1));
  auto out = m.Collect(0);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].ticks.size(), 1u);
}
