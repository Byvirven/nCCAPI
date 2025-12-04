#ifndef NCCAPI_BITMEX_SESSION_HPP
#define NCCAPI_BITMEX_SESSION_HPP

#include "ccapi_cpp/ccapi_session_configs.h"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_subscription.h"

namespace ccapi {
    class Session;
    class EventHandler;
    class Request;
    template<typename T> class Queue;
    class Event;
}

#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_queue.h"

namespace nccapi {

class BitmexSession {
public:
    BitmexSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler = nullptr);
    ~BitmexSession();

    void sendRequest(ccapi::Request& request);
    void stop();
    ccapi::Queue<ccapi::Event>& getEventQueue();

private:
    ccapi::Session* session;
};

}

#endif
