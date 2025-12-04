#include "nccapi/sessions/erisx_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_ERISX
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

ErisxSession::ErisxSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

ErisxSession::~ErisxSession() {
    delete session;
}

void ErisxSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void ErisxSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& ErisxSession::getEventQueue() {
    return session->getEventQueue();
}

}
