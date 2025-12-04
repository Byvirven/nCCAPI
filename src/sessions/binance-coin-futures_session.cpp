#include "nccapi/sessions/binance-coin-futures_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BinanceCoinFuturesSession::BinanceCoinFuturesSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BinanceCoinFuturesSession::~BinanceCoinFuturesSession() {
    delete session;
}

void BinanceCoinFuturesSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BinanceCoinFuturesSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BinanceCoinFuturesSession::getEventQueue() {
    return session->getEventQueue();
}

}
