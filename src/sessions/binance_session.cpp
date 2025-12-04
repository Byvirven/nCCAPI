#include "nccapi/sessions/binance_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BINANCE
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BinanceSession::BinanceSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BinanceSession::~BinanceSession() {
    delete session;
}

void BinanceSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BinanceSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BinanceSession::getEventQueue() {
    return session->getEventQueue();
}

}
