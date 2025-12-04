#include "nccapi/sessions/gateio-perpetual-futures_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

GateioPerpetualFuturesSession::GateioPerpetualFuturesSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

GateioPerpetualFuturesSession::~GateioPerpetualFuturesSession() {
    delete session;
}

void GateioPerpetualFuturesSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void GateioPerpetualFuturesSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& GateioPerpetualFuturesSession::getEventQueue() {
    return session->getEventQueue();
}

}
