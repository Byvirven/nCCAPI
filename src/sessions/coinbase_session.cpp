#include "nccapi/sessions/coinbase_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_COINBASE
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

CoinbaseSession::CoinbaseSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

CoinbaseSession::~CoinbaseSession() {
    delete session;
}

void CoinbaseSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void CoinbaseSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& CoinbaseSession::getEventQueue() {
    return session->getEventQueue();
}

}
