#include "nccapi/sessions/bitmex_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BITMEX
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BitmexSession::BitmexSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BitmexSession::~BitmexSession() {
    delete session;
}

void BitmexSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BitmexSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BitmexSession::getEventQueue() {
    return session->getEventQueue();
}

}
