// In-process mtserver + mtcomm with two mtclient sessions: subscription
// sharing, cache-served initial paint, upstream loss and recovery with recap.
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <mtclient_session.h>
#include <mtcomm_config.h>
#include <mtcomm_engine.h>
#include <mtcore_mutex.h>
#include <mtcore_socket.h>
#include <mtcore_thread.h>
#include <mtcore_time.h>
#include <mtsrv_config.h>
#include <mtsrv_server.h>

using mtcore::Message;
using mtcore::MessageFlags;
using mtcore::MessageType;

namespace {

const int k_SERVER_PORT = 38195;
const int k_COMMS_PORT  = 38194;

class Collector : public mtclient::EventHandler {
  public:
    mtcore::Mutex        d_mutex;
    std::vector<Message> d_messages;
    std::vector<int>     d_states;

    virtual void processMessage(mtclient::Session *, const Message& message)
    {
        mtcore::LockGuard guard(&d_mutex);
        d_messages.push_back(message);
    }
    virtual void processSessionState(mtclient::Session *,
                                     mtclient::SessionState::Value state,
                                     const Message *)
    {
        mtcore::LockGuard guard(&d_mutex);
        d_states.push_back(state);
    }

    int count(MessageType::Value type, mtcore::Uint32 flag = 0,
              const std::string& status = "")
    {
        mtcore::LockGuard guard(&d_mutex);
        int n = 0;
        for (size_t i = 0; i < d_messages.size(); ++i) {
            const Message& m = d_messages[i];
            if (m.type() != type) continue;
            if (flag != 0 && !m.hasFlag(flag)) continue;
            if (!status.empty()
             && m.body().getChildString("status") != status) continue;
            ++n;
        }
        return n;
    }

    bool lastOf(Message *result, MessageType::Value type)
    {
        mtcore::LockGuard guard(&d_mutex);
        for (size_t i = d_messages.size(); i > 0; --i) {
            if (d_messages[i - 1].type() == type) {
                *result = d_messages[i - 1];
                return true;
            }
        }
        return false;
    }
};

template <class PRED>
bool waitUntil(PRED pred, int timeoutMs = 15000)
{
    mtcore::Int64 end = mtcore::TimeUtil::nowMillis() + timeoutMs;
    while (mtcore::TimeUtil::nowMillis() < end) {
        if (pred()) return true;
        mtcore::Thread::sleep(20);
    }
    return pred();
}

std::unique_ptr<mtsrv::Server> startServer()
{
    mtsrv::ServerConfig config;
    config.port = k_SERVER_PORT;
    config.seed = 42;
    config.tickIntervalMs = 20;
    config.tickProbability = 1.0;
    std::unique_ptr<mtsrv::Server> server(new mtsrv::Server(config));
    EXPECT_EQ(0, server->start());
    return server;
}

std::unique_ptr<mtclient::Session> connect(Collector *collector,
                                           const char *user)
{
    mtclient::SessionOptions options;
    options.port = k_COMMS_PORT;
    options.user = user;
    options.application = "backend_integration_test";
    std::unique_ptr<mtclient::Session> session(
                                 new mtclient::Session(options, collector));
    EXPECT_EQ(0, session->start());
    return session;
}

}  // namespace

class BackendIntegration : public ::testing::Test {
  protected:
    static void SetUpTestSuite() { mtcore::SocketUtil::initialize(); }
    static void TearDownTestSuite() { mtcore::SocketUtil::cleanup(); }
};

TEST_F(BackendIntegration, SharesSubscriptionsAndRecoversFromServerRestart)
{
    std::unique_ptr<mtsrv::Server> server = startServer();

    mtcomm::CommsConfig config;
    config.port = k_COMMS_PORT;
    config.upstreamPort = k_SERVER_PORT;
    config.reconnectMinMs = 100;
    config.reconnectMaxMs = 500;
    mtcomm::CommsEngine engine(config);
    ASSERT_EQ(0, engine.start());

    Collector a;
    Collector b;
    std::unique_ptr<mtclient::Session> sa = connect(&a, "ALICE");
    std::unique_ptr<mtclient::Session> sb = connect(&b, "BOB");
    ASSERT_TRUE(waitUntil([&] { return sa->isUp() && sb->isUp(); }));

    std::vector<std::string> fields;
    fields.push_back("LAST_PRICE");
    fields.push_back("BID");
    sa->subscribe("//mt/mktdata", "AAPL US Equity", fields, 1);
    ASSERT_TRUE(waitUntil([&] {
        return a.count(MessageType::e_SUBSCRIPTION_DATA,
                       MessageFlags::e_INITIAL_PAINT) == 1;
    }));

    // Same topic, different field order: served from mtcomm's cache and
    // multiplexed onto the one upstream subscription.
    fields[0] = "BID";
    fields[1] = "LAST_PRICE";
    sb->subscribe("//mt/mktdata", "AAPL US Equity", fields, 1);
    ASSERT_TRUE(waitUntil([&] {
        return b.count(MessageType::e_SUBSCRIPTION_DATA,
                       MessageFlags::e_INITIAL_PAINT) == 1;
    }));
    ASSERT_TRUE(waitUntil([&] {
        return b.count(MessageType::e_SUBSCRIPTION_DATA) >= 3;
    }));

    mtcore::Element status;
    engine.buildStatus(&status);
    const mtcore::Element *topics = status.getChild("topics");
    ASSERT_TRUE(topics != NULL);
    ASSERT_EQ(1u, topics->numChildren());
    EXPECT_EQ(2, topics->childAt(0)->getChildInt64("subscribers"));
    EXPECT_GE(status.getChild("counters")->getChildInt64("cacheHits"), 1);

    // Upstream outage: clients stay connected to mtcomm, see UPSTREAM_DOWN
    // and the cache goes stale.
    server->stop();
    server.reset();
    ASSERT_TRUE(waitUntil([&] {
        return a.count(MessageType::e_SESSION_STATUS, 0, "UPSTREAM_DOWN") >= 1
            && b.count(MessageType::e_SESSION_STATUS, 0, "UPSTREAM_DOWN") >= 1;
    }));
    EXPECT_TRUE(sa->isUp());
    status.clear();
    engine.buildStatus(&status);
    EXPECT_EQ(1, status.getChildInt64("staleEntries"));

    // A new server comes up on the same port: mtcomm reconnects, restores
    // the subscription and fans the fresh image out as a RECAP.
    server = startServer();
    ASSERT_TRUE(waitUntil([&] {
        return a.count(MessageType::e_SESSION_STATUS, 0, "UPSTREAM_UP") >= 1
            && a.count(MessageType::e_SUBSCRIPTION_DATA,
                       MessageFlags::e_RECAP) >= 1
            && b.count(MessageType::e_SUBSCRIPTION_DATA,
                       MessageFlags::e_RECAP) >= 1;
    }));
    status.clear();
    engine.buildStatus(&status);
    EXPECT_EQ(0, status.getChildInt64("staleEntries"));
    EXPECT_EQ(1, status.getChild("topics")->numChildren());

    sa->stop();
    sb->stop();
    engine.stop();
    server->stop();
}

TEST_F(BackendIntegration, RoutesRequestsAndFailures)
{
    std::unique_ptr<mtsrv::Server> server = startServer();
    mtcomm::CommsConfig config;
    config.port = k_COMMS_PORT;
    config.upstreamPort = k_SERVER_PORT;
    mtcomm::CommsEngine engine(config);
    ASSERT_EQ(0, engine.start());

    Collector c;
    std::unique_ptr<mtclient::Session> s = connect(&c, "ME");
    ASSERT_TRUE(waitUntil([&] { return s->isUp(); }));

    Message ok(MessageType::e_REQUEST, s->nextCorrelationId());
    ok.setService("//mt/refdata");
    ok.setOperation("SecurityList");
    s->sendRequest(ok);
    Message bad(MessageType::e_REQUEST, s->nextCorrelationId());
    bad.setService("//mt/refdata");
    bad.setOperation("HistoricalData");
    bad.body().setChildString("security", "NOPE US Equity");
    s->sendRequest(bad);
    Message admin(MessageType::e_REQUEST, s->nextCorrelationId());
    admin.setService("//mtcomm/admin");
    admin.setOperation("Status");
    s->sendRequest(admin);

    ASSERT_TRUE(waitUntil([&] {
        return c.count(MessageType::e_RESPONSE) == 2
            && c.count(MessageType::e_REQUEST_FAILURE) == 1;
    }));
    Message failure;
    ASSERT_TRUE(c.lastOf(&failure, MessageType::e_REQUEST_FAILURE));
    EXPECT_EQ(bad.correlationId(), failure.correlationId());

    s->stop();
    engine.stop();
    server->stop();
}
