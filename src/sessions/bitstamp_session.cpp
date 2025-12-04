#include "nccapi/sessions/bitstamp_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BITSTAMP
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BitstampSession::BitstampSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BitstampSession::~BitstampSession() {
    delete session;
}

void BitstampSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BitstampSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BitstampSession::getEventQueue() {
    return session->getEventQueue();
}

}
