#include "nccapi/sessions/kucoin_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_KUCOIN
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

KucoinSession::KucoinSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

KucoinSession::~KucoinSession() {
    delete session;
}

void KucoinSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void KucoinSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& KucoinSession::getEventQueue() {
    return session->getEventQueue();
}

}
