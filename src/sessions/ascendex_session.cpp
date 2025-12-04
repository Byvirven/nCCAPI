#include "nccapi/sessions/ascendex_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_ASCENDEX
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

AscendexSession::AscendexSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

AscendexSession::~AscendexSession() {
    delete session;
}

void AscendexSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void AscendexSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& AscendexSession::getEventQueue() {
    return session->getEventQueue();
}

}
