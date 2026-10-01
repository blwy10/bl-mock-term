#include <gtest/gtest.h>

#include <cmath>

#include "instruments.h"
#include "simulator.h"

using namespace mockterm::md;

namespace {
std::vector<Instrument> Universe() { return DefaultInstruments(); }
}  // namespace

TEST(Instruments, UniverseCoversAllAssetClasses) {
  const auto& u = DefaultInstruments();
  EXPECT_GE(u.size(), 30u);
  int counts[4] = {};
  for (const auto& i : u) counts[static_cast<int>(i.asset_class)]++;
  EXPECT_GT(counts[static_cast<int>(AssetClass::Equity)], 0);
  EXPECT_GT(counts[static_cast<int>(AssetClass::Fx)], 0);
  EXPECT_GT(counts[static_cast<int>(AssetClass::Index)], 0);
  EXPECT_GE(counts[static_cast<int>(AssetClass::Rate)], 2);
}

TEST(TickSimulator, SameSeedProducesSameSequence) {
  TickSimulator a(Universe(), 42), b(Universe(), 42);
  for (int step = 0; step < 50; ++step) {
    auto ta = a.Step(1000 + step), tb = b.Step(1000 + step);
    ASSERT_EQ(ta.size(), tb.size());
    for (size_t i = 0; i < ta.size(); ++i) {
      EXPECT_EQ(ta[i].ticker, tb[i].ticker);
      EXPECT_DOUBLE_EQ(ta[i].last, tb[i].last);
    }
  }
}

TEST(TickSimulator, TicksAreInternallyConsistent) {
  TickSimulator sim(Universe(), 7);
  uint64_t last_seq = 0;
  for (int step = 0; step < 2000; ++step) {
    for (const Tick& t : sim.Step(step * 25, 0.3)) {
      const Instrument* inst = sim.Find(t.ticker);
      ASSERT_NE(inst, nullptr);
      EXPECT_GT(t.last, 0);
      EXPECT_LT(t.bid, t.ask) << t.ticker;
      EXPECT_LE(t.low, t.last);
      EXPECT_GE(t.high, t.last);
      EXPECT_NEAR(t.change, t.last - t.prev_close, std::pow(10.0, -inst->decimals) * 1.01);
      EXPECT_NEAR(t.change_pct, (t.last - t.prev_close) / t.prev_close * 100.0, 1e-9);
      EXPECT_EQ(t.ts, step * 25);
      EXPECT_GT(t.seq, last_seq);
      last_seq = t.seq;
    }
  }
}

TEST(TickSimulator, PricesAreRoundedToInstrumentDecimals) {
  TickSimulator sim(Universe(), 9);
  for (int step = 0; step < 200; ++step) {
    for (const Tick& t : sim.Step(step, 0.5)) {
      double f = std::pow(10.0, sim.Find(t.ticker)->decimals);
      EXPECT_NEAR(t.last * f, std::round(t.last * f), 1e-6) << t.ticker;
      EXPECT_NEAR(t.bid * f, std::round(t.bid * f), 1e-6) << t.ticker;
    }
  }
}

TEST(TickSimulator, EquityVolumeIsMonotonic) {
  TickSimulator sim(Universe(), 11);
  int64_t prev = sim.Snapshot("AAPL US Equity")->volume;
  for (int i = 0; i < 100; ++i) {
    auto t = sim.TickOne("AAPL US Equity", i);
    ASSERT_TRUE(t.has_value());
    EXPECT_GT(t->volume, prev);
    prev = t->volume;
  }
}

TEST(TickSimulator, RandomWalkStaysNearReference) {
  TickSimulator sim(Universe(), 13);
  for (int i = 0; i < 20000; ++i) sim.TickOne("SPX Index", i);
  const Instrument* spx = sim.Find("SPX Index");
  double last = sim.Snapshot("SPX Index")->last;
  EXPECT_GT(last, spx->base_price * 0.8);
  EXPECT_LT(last, spx->base_price * 1.2);
}

TEST(TickSimulator, UnknownTickerReturnsNullopt) {
  TickSimulator sim(Universe(), 1);
  EXPECT_FALSE(sim.Snapshot("NOPE Equity").has_value());
  EXPECT_FALSE(sim.TickOne("NOPE Equity", 0).has_value());
  EXPECT_EQ(sim.Find("NOPE Equity"), nullptr);
}

TEST(TickSimulator, StepProbabilityBoundsTickCount) {
  TickSimulator sim(Universe(), 3);
  EXPECT_TRUE(sim.Step(0, 0.0).empty());
  EXPECT_EQ(sim.Step(1, 1.0).size(), Universe().size());
}
