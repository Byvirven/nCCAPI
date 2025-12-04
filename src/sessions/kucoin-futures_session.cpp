#include "nccapi/sessions/kucoin-futures_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

KucoinFuturesSession::KucoinFuturesSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

KucoinFuturesSession::~KucoinFuturesSession() {
    delete session;
}

void KucoinFuturesSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void KucoinFuturesSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& KucoinFuturesSession::getEventQueue() {
    return session->getEventQueue();
}

}
