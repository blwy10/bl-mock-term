// Unit tests for the legacy back end: wire codec, element model, ticker-plant
// gap detection and the mtcomm cache/conflation/multiplexing structures.
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include <mtcomm_conflator.h>
#include <mtcomm_lvc.h>
#include <mtcomm_topicmux.h>
#include <mtcore_codec.h>
#include <mtcore_element.h>
#include <mtcore_message.h>
#include <mtsrv_gapdetector.h>

using mtcore::Element;
using mtcore::Message;
using mtcore::MessageCodec;
using mtcore::MessageFlags;
using mtcore::MessageType;

namespace {

Message subscribeMsg(const std::string& topic, const char *f1, const char *f2)
{
    Message msg(MessageType::e_SUBSCRIBE, 1);
    msg.setService("//mt/mktdata");
    msg.setTopic(topic);
    Element *fields = msg.body().addChild("fields");
    fields->setArray(true);
    fields->appendItem()->setString(f1);
    fields->appendItem()->setString(f2);
    return msg;
}

}  // namespace

TEST(Codec, RoundTripsNestedMessage)
{
    Message in(MessageType::e_SUBSCRIPTION_DATA, 0x1122334455667788ULL);
    in.setService("//mt/mktdata");
    in.setTopic("AAPL US Equity");
    in.setFlags(MessageFlags::e_INITIAL_PAINT | MessageFlags::e_STALE);
    in.body().setChildFloat64("LAST_PRICE", 189.25);
    in.body().setChildInt64("VOLUME", -5);
    in.body().setChildString("NAME", "Apple Inc");
    in.body().setChildDatetime("LAST_UPDATE_TIME", 1700000000123LL);
    Element *list = in.body().addChild("tickers");
    list->setArray(true);
    list->appendItem()->setString("A");
    list->appendItem()->setString("B");

    std::string frame;
    MessageCodec::encode(&frame, in);
    mtcore::Uint32 length = 0;
    ASSERT_EQ(0, MessageCodec::frameLength(&length, frame.data()));
    ASSERT_EQ(frame.size() - 4, length);

    Message out;
    ASSERT_EQ(0, MessageCodec::decode(&out, frame.data() + 4, length));
    EXPECT_EQ(MessageType::e_SUBSCRIPTION_DATA, out.type());
    EXPECT_EQ(0x1122334455667788ULL, out.correlationId());
    EXPECT_TRUE(out.hasFlag(MessageFlags::e_STALE));
    EXPECT_FALSE(out.hasFlag(MessageFlags::e_RECAP));
    EXPECT_EQ("AAPL US Equity", out.topic());
    EXPECT_DOUBLE_EQ(189.25, out.body().getChildFloat64("LAST_PRICE"));
    EXPECT_EQ(-5, out.body().getChildInt64("VOLUME"));
    EXPECT_EQ("Apple Inc", out.body().getChildString("NAME"));
    EXPECT_EQ(1700000000123LL, out.body().getChildInt64("LAST_UPDATE_TIME"));
    ASSERT_TRUE(out.body().getChild("tickers") != NULL);
    EXPECT_EQ(2u, out.body().getChild("tickers")->numChildren());
}

TEST(Codec, RejectsMalformedPayloads)
{
    Message in(MessageType::e_HEARTBEAT, 1);
    std::string frame;
    MessageCodec::encode(&frame, in);
    Message out;
    for (size_t cut = 0; cut + 4 < frame.size(); ++cut) {
        EXPECT_NE(0, MessageCodec::decode(&out, frame.data() + 4, cut))
            << "truncated at " << cut;
    }
    std::string bad = frame;
    bad[4] = 'X';
    EXPECT_NE(0, MessageCodec::decode(&out, bad.data() + 4, bad.size() - 4));

    const char huge[4] = { '\x7f', '\x00', '\x00', '\x00' };
    mtcore::Uint32 length = 0;
    EXPECT_NE(0, MessageCodec::frameLength(&length, huge));
}

TEST(Element, MergeOverwritesAndAddsFields)
{
    Element image;
    image.setChildFloat64("BID", 1);
    image.setChildFloat64("ASK", 2);
    Element delta;
    delta.setChildFloat64("ASK", 3);
    delta.setChildInt64("VOLUME", 9);
    image.merge(delta);
    EXPECT_DOUBLE_EQ(1, image.getChildFloat64("BID"));
    EXPECT_DOUBLE_EQ(3, image.getChildFloat64("ASK"));
    EXPECT_EQ(9, image.getChildInt64("VOLUME"));

    Element copy(image);
    copy.setChildFloat64("BID", 7);
    EXPECT_DOUBLE_EQ(1, image.getChildFloat64("BID")) << "copies are deep";
}

TEST(GapDetector, FlagsGapsAndDuplicatesPerFeed)
{
    mtsrv::GapDetector detector;
    EXPECT_EQ(mtsrv::SequenceCheck::e_OK, detector.check("NYSE", 1));
    EXPECT_EQ(mtsrv::SequenceCheck::e_OK, detector.check("NYSE", 2));
    EXPECT_EQ(mtsrv::SequenceCheck::e_GAP, detector.check("NYSE", 5));
    EXPECT_EQ(mtsrv::SequenceCheck::e_DUPLICATE, detector.check("NYSE", 4));
    EXPECT_EQ(mtsrv::SequenceCheck::e_OK, detector.check("NYSE", 6));
    EXPECT_EQ(mtsrv::SequenceCheck::e_OK, detector.check("LSE", 1));
}

TEST(LastValueCache, MergesUpdatesAndTracksStaleness)
{
    mtcomm::LastValueCache cache;
    Element paint;
    paint.setChildFloat64("BID", 1);
    paint.setChildFloat64("ASK", 2);
    cache.update("k", paint, true, 100);
    Element delta;
    delta.setChildFloat64("ASK", 2.5);
    cache.update("k", delta, false, 200);

    Element image;
    bool stale = true;
    ASSERT_TRUE(cache.get(&image, &stale, "k"));
    EXPECT_FALSE(stale);
    EXPECT_DOUBLE_EQ(1, image.getChildFloat64("BID"));
    EXPECT_DOUBLE_EQ(2.5, image.getChildFloat64("ASK"));

    cache.markAllStale();
    EXPECT_EQ(1, cache.staleCount());
    ASSERT_TRUE(cache.get(&image, &stale, "k"));
    EXPECT_TRUE(stale);

    Element fresh;
    fresh.setChildFloat64("BID", 4);
    cache.update("k", fresh, true, 300);
    EXPECT_EQ(0, cache.staleCount());
    ASSERT_TRUE(cache.get(&image, "k"));
    EXPECT_FALSE(image.hasChild("ASK")) << "a recap replaces the image";
    EXPECT_FALSE(cache.get(&image, "missing"));
}

TEST(Conflator, MergesUpdatesUntilIntervalElapses)
{
    mtcomm::Conflator conflator;
    mtcomm::DownstreamKey key(7, 1);
    Element a;
    a.setChildFloat64("BID", 1);
    a.setChildFloat64("ASK", 2);
    Element b;
    b.setChildFloat64("BID", 1.5);
    conflator.add(key, "//mt/mktdata", "X", a, 1000);
    conflator.add(key, "//mt/mktdata", "X", b, 1010);
    EXPECT_EQ(1, conflator.pendingCount());

    std::map<mtcore::Uint64, int> intervals;
    intervals[7] = 100;
    std::vector<mtcomm::ConflatedUpdate> out;
    conflator.drain(&out, 1050, intervals);
    EXPECT_TRUE(out.empty());
    conflator.drain(&out, 1100, intervals);
    ASSERT_EQ(1u, out.size());
    EXPECT_EQ(2, out[0].count);
    EXPECT_DOUBLE_EQ(1.5, out[0].fields.getChildFloat64("BID"));
    EXPECT_DOUBLE_EQ(2, out[0].fields.getChildFloat64("ASK"));
    EXPECT_EQ(0, conflator.pendingCount());

    conflator.add(key, "//mt/mktdata", "X", a, 2000);
    EXPECT_EQ(1, conflator.removeClient(7));
}

TEST(TopicMux, SharesEquivalentSubscriptions)
{
    EXPECT_EQ(mtcomm::TopicMux::makeKey(subscribeMsg("AAPL US Equity", "bid", "ASK")),
              mtcomm::TopicMux::makeKey(subscribeMsg("AAPL US Equity", "ASK", "BID")));

    mtcomm::TopicMux mux;
    bool created = false;
    mtcomm::UpstreamTopic *t1 = mux.attach(&created, mtcomm::DownstreamKey(1, 10),
                                           subscribeMsg("AAPL US Equity", "BID", "ASK"), 500);
    ASSERT_TRUE(t1 != NULL);
    EXPECT_TRUE(created);
    mtcomm::UpstreamTopic *t2 = mux.attach(&created, mtcomm::DownstreamKey(2, 10),
                                           subscribeMsg("AAPL US Equity", "ASK", "BID"), 501);
    EXPECT_EQ(t1, t2);
    EXPECT_FALSE(created);
    EXPECT_EQ(1, mux.topicCount());
    EXPECT_EQ(2, mux.subscriberCount());
    EXPECT_TRUE(mux.attach(&created, mtcomm::DownstreamKey(1, 10),
                           subscribeMsg("MSFT US Equity", "BID", "ASK"), 502) == NULL);
    EXPECT_EQ(t1, mux.findByUpstream(500));

    mtcomm::RemovedTopic removed;
    EXPECT_FALSE(mux.detach(&removed, mtcomm::DownstreamKey(1, 10)));
    EXPECT_TRUE(mux.detach(&removed, mtcomm::DownstreamKey(2, 10)));
    EXPECT_EQ(500u, removed.upstreamCid);
    EXPECT_EQ(0, mux.topicCount());
}
