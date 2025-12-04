#include "nccapi/sessions/bitmart_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BITMART
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BitmartSession::BitmartSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BitmartSession::~BitmartSession() {
    delete session;
}

void BitmartSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BitmartSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BitmartSession::getEventQueue() {
    return session->getEventQueue();
}

}
