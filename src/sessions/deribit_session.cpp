#include "nccapi/sessions/deribit_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_DERIBIT
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

DeribitSession::DeribitSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

DeribitSession::~DeribitSession() {
    delete session;
}

void DeribitSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void DeribitSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& DeribitSession::getEventQueue() {
    return session->getEventQueue();
}

}
