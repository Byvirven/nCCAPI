#include "nccapi/sessions/bitfinex_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BITFINEX
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BitfinexSession::BitfinexSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BitfinexSession::~BitfinexSession() {
    delete session;
}

void BitfinexSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BitfinexSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BitfinexSession::getEventQueue() {
    return session->getEventQueue();
}

}
