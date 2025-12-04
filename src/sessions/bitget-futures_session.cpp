#include "nccapi/sessions/bitget-futures_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BitgetFuturesSession::BitgetFuturesSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BitgetFuturesSession::~BitgetFuturesSession() {
    delete session;
}

void BitgetFuturesSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BitgetFuturesSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BitgetFuturesSession::getEventQueue() {
    return session->getEventQueue();
}

}
