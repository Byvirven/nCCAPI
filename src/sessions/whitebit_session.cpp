#include "nccapi/sessions/whitebit_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_WHITEBIT
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

WhitebitSession::WhitebitSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

WhitebitSession::~WhitebitSession() {
    delete session;
}

void WhitebitSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void WhitebitSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& WhitebitSession::getEventQueue() {
    return session->getEventQueue();
}

}
