#include "nccapi/sessions/mexc_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_MEXC
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

MexcSession::MexcSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

MexcSession::~MexcSession() {
    delete session;
}

void MexcSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void MexcSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& MexcSession::getEventQueue() {
    return session->getEventQueue();
}

}
