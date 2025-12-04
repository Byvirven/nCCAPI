#include "nccapi/sessions/kraken-futures_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

KrakenFuturesSession::KrakenFuturesSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

KrakenFuturesSession::~KrakenFuturesSession() {
    delete session;
}

void KrakenFuturesSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void KrakenFuturesSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& KrakenFuturesSession::getEventQueue() {
    return session->getEventQueue();
}

}
