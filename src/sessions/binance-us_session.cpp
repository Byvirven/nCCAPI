#include "nccapi/sessions/binance-us_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BINANCE_US
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BinanceUsSession::BinanceUsSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BinanceUsSession::~BinanceUsSession() {
    delete session;
}

void BinanceUsSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BinanceUsSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BinanceUsSession::getEventQueue() {
    return session->getEventQueue();
}

}
