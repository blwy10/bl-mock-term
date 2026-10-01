// mtsrv_service.h                                                    -*-C++-*-
#ifndef INCLUDED_MTSRV_SERVICE
#define INCLUDED_MTSRV_SERVICE

//@PURPOSE: Provide the protocol for server-side services and reply helpers.
//
//@CLASSES:
//  mtsrv::Service:      abstract request/subscription service
//  mtsrv::ServiceUtil:  helpers for building replies and failures
//
//@DESCRIPTION: The server routes each 'e_REQUEST' to a worker thread that
// calls 'handleRequest', and each 'e_SUBSCRIBE' / 'e_UNSUBSCRIBE' directly
// from the connection's reader thread.  Implementations must be thread-safe.

#include "mtsrv_clientsession.h"

#include <mtcore_message.h>

#include <string>

namespace mtsrv {

class Service {
  public:
    virtual ~Service();

    virtual const char *name() const = 0;
        // e.g. "//mt/mktdata"
    virtual const char *description() const = 0;

    virtual void handleRequest(ClientSession         *session,
                               const mtcore::Message& request);
        // Default: reply 'e_REQUEST_FAILURE' "operation not supported".
    virtual void handleSubscribe(ClientSession         *session,
                                 const mtcore::Message& subscribe);
        // Default: reply 'e_SUBSCRIPTION_FAILURE'.
    virtual void handleUnsubscribe(ClientSession *session,
                                   Uint64         correlationId);
    virtual void sessionClosed(ClientSession *session);

    virtual void appendStatistics(mtcore::Element *stats) const;
    virtual void describeOperations(mtcore::Element *operations) const;
};

struct ErrorCode {
    enum Value {
        e_UNKNOWN_SERVICE   = 100,
        e_UNKNOWN_OPERATION = 101,
        e_BAD_ARGUMENT      = 102,
        e_UNKNOWN_SECURITY  = 103,
        e_UNKNOWN_FIELD     = 104,
        e_NOT_LOGGED_ON     = 105,
        e_DUPLICATE_ID      = 106,
        e_INTERNAL          = 199
    };

    static const char *category(Value value);
};

struct ServiceUtil {
    static void initReply(mtcore::Message           *reply,
                          mtcore::MessageType::Value type,
                          const mtcore::Message&     original);
        // Copy correlation id, service, operation and topic.

    static void sendRequestFailure(ClientSession         *session,
                                   const mtcore::Message& request,
                                   ErrorCode::Value       code,
                                   const std::string&     text);
    static void sendSubscriptionFailure(ClientSession         *session,
                                        const mtcore::Message& subscribe,
                                        ErrorCode::Value       code,
                                        const std::string&     text);
    static void fillError(mtcore::Element   *reason,
                          ErrorCode::Value   code,
                          const std::string& text);
};

}  // close namespace mtsrv

#endif
