#include "nccapi/sessions/gateio_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_GATEIO
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

GateioSession::GateioSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

GateioSession::~GateioSession() {
    delete session;
}

void GateioSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void GateioSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& GateioSession::getEventQueue() {
    return session->getEventQueue();
}

}
