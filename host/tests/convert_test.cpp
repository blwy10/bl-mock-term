#include <gtest/gtest.h>

#include <mtcore_element.h>

#include "convert.h"
#include "json.h"
#include "types.h"

using namespace mockterm::md;

TEST(Convert, AppliesPartialTickUpdatesOverRunningImage) {
  mtcore::Element paint;
  paint.setChildFloat64("LAST_PRICE", 100.5);
  paint.setChildFloat64("BID", 100.4);
  paint.setChildFloat64("ASK", 100.6);
  paint.setChildInt64("VOLUME", 1000);
  paint.setChildDatetime("LAST_UPDATE_TIME", 1700000000000);
  paint.setChildInt64("SEQUENCE_NUMBER", 7);

  Tick t;
  t.ticker = "IBM US Equity";
  convert::ApplyTickFields(&t, paint);
  EXPECT_DOUBLE_EQ(t.last, 100.5);
  EXPECT_DOUBLE_EQ(t.bid, 100.4);
  EXPECT_EQ(t.volume, 1000);
  EXPECT_EQ(t.ts, 1700000000000);
  EXPECT_EQ(t.seq, 7u);

  mtcore::Element delta;
  delta.setChildFloat64("LAST_PRICE", 101.0);
  convert::ApplyTickFields(&t, delta);
  EXPECT_DOUBLE_EQ(t.last, 101.0);
  EXPECT_DOUBLE_EQ(t.bid, 100.4) << "absent fields keep their previous value";
  EXPECT_EQ(t.ticker, "IBM US Equity");
}

TEST(Convert, HistoryBars) {
  mtcore::Element body;
  mtcore::Element* bars = body.addChild("bars");
  bars->setArray(true);
  for (int i = 0; i < 3; ++i) {
    mtcore::Element* b = bars->appendItem();
    b->setChildDatetime("time", 1000 + i * 60000);
    b->setChildFloat64("open", 1 + i);
    b->setChildFloat64("high", 2 + i);
    b->setChildFloat64("low", 0.5 + i);
    b->setChildFloat64("close", 1.5 + i);
    b->setChildInt64("volume", 10 * i);
  }
  std::vector<Bar> out = convert::BarsFromHistory(body);
  ASSERT_EQ(out.size(), 3u);
  EXPECT_EQ(out[2].ts, 121000);
  EXPECT_DOUBLE_EQ(out[2].close, 3.5);
  EXPECT_EQ(out[2].volume, 20);
  EXPECT_TRUE(convert::BarsFromHistory(mtcore::Element()).empty());
}

TEST(Convert, HeadlineAndChat) {
  mtcore::Element h;
  h.setChildInt64("id", 42);
  h.setChildDatetime("time", 5);
  h.setChildString("source", "MTN");
  h.setChildString("category", "Equity");
  mtcore::Element* tickers = h.addChild("tickers");
  tickers->setArray(true);
  tickers->appendItem()->setString("AAPL US Equity");
  h.setChildString("headline", "Apple rises");
  h.setChildString("body", "text");
  Headline out = convert::HeadlineFrom(h);
  EXPECT_EQ(out.id, 42u);
  EXPECT_EQ(out.tickers, std::vector<std::string>{"AAPL US Equity"});
  EXPECT_EQ(out.headline, "Apple rises");

  mtcore::Element c;
  c.setChildInt64("id", 1);
  c.setChildString("from", "ME");
  c.setChildString("to", "ALICE");
  c.setChildString("text", "hi");
  EXPECT_TRUE(convert::ChatFrom(c, "ME").outgoing);
  EXPECT_FALSE(convert::ChatFrom(c, "ALICE").outgoing);
  EXPECT_NE(json::ToJson(convert::ChatFrom(c, "ME")).find("\"direction\":\"out\""), std::string::npos);
}

TEST(Convert, InstrumentsKeepRendererShape) {
  mtcore::Element body;
  mtcore::Element* list = body.addChild("securities");
  list->setArray(true);
  mtcore::Element* eq = list->appendItem();
  eq->setChildString("ticker", "AAPL US Equity");
  eq->setChildString("name", "Apple Inc");
  eq->setChildString("assetClass", "Equity");
  eq->setChildInt64("decimals", 2);
  mtcore::Element* f = eq->addChild("fundamentals");
  f->setChildString("sector", "Technology");
  f->setChildFloat64("pe", 30);
  mtcore::Element* fx = list->appendItem();
  fx->setChildString("ticker", "EURUSD Curncy");
  fx->setChildString("assetClass", "Curncy");
  fx->setChildInt64("decimals", 4);

  std::vector<Instrument> out = convert::InstrumentsFrom(body);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].asset_class, AssetClass::Equity);
  EXPECT_EQ(out[0].fundamentals.sector, "Technology");
  EXPECT_EQ(out[1].asset_class, AssetClass::Fx);
  EXPECT_EQ(out[1].decimals, 4);
  const std::string js = json::ToJson(out);
  EXPECT_NE(js.find("\"assetClass\":\"Curncy\""), std::string::npos);
  EXPECT_NE(js.find("\"fundamentals\":{\"sector\":\"Technology\""), std::string::npos);
}

TEST(Convert, ErrorText) {
  mtcore::Element body;
  mtcore::Element* reason = body.addChild("reason");
  reason->setChildString("category", "BAD_ARGS");
  reason->setChildString("message", "unknown security: X");
  EXPECT_EQ(convert::ErrorText(body), "BAD_ARGS: unknown security: X");
  EXPECT_EQ(convert::ErrorText(mtcore::Element()), "request failed");
}

TEST(Types, ParsersRoundTrip) {
  for (HistoryRange r : {HistoryRange::OneDay, HistoryRange::OneMonth, HistoryRange::OneYear}) {
    HistoryRange back;
    ASSERT_TRUE(ParseHistoryRange(ToString(r), &back));
    EXPECT_EQ(back, r);
  }
  AssetClass c;
  EXPECT_FALSE(ParseAssetClass("Bond", &c));
  EXPECT_STREQ(ToString(ConnectionState::Stale), "stale");
}
