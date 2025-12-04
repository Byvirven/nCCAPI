#include "nccapi/sessions/mexc-futures_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

MexcFuturesSession::MexcFuturesSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

MexcFuturesSession::~MexcFuturesSession() {
    delete session;
}

void MexcFuturesSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void MexcFuturesSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& MexcFuturesSession::getEventQueue() {
    return session->getEventQueue();
}

}
