#include "nccapi/sessions/cryptocom_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_CRYPTOCOM
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

CryptocomSession::CryptocomSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

CryptocomSession::~CryptocomSession() {
    delete session;
}

void CryptocomSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void CryptocomSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& CryptocomSession::getEventQueue() {
    return session->getEventQueue();
}

}
