#include "nccapi/sessions/okx_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_OKX
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

OkxSession::OkxSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

OkxSession::~OkxSession() {
    delete session;
}

void OkxSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void OkxSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& OkxSession::getEventQueue() {
    return session->getEventQueue();
}

}
