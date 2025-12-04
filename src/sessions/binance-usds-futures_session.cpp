#include "nccapi/sessions/binance-usds-futures_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BinanceUsdsFuturesSession::BinanceUsdsFuturesSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BinanceUsdsFuturesSession::~BinanceUsdsFuturesSession() {
    delete session;
}

void BinanceUsdsFuturesSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BinanceUsdsFuturesSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BinanceUsdsFuturesSession::getEventQueue() {
    return session->getEventQueue();
}

}
