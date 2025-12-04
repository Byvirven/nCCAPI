#include "nccapi/sessions/bybit_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BYBIT
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BybitSession::BybitSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BybitSession::~BybitSession() {
    delete session;
}

void BybitSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BybitSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BybitSession::getEventQueue() {
    return session->getEventQueue();
}

}
