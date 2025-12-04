#include "nccapi/sessions/bitget_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_BITGET
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

BitgetSession::BitgetSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

BitgetSession::~BitgetSession() {
    delete session;
}

void BitgetSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void BitgetSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& BitgetSession::getEventQueue() {
    return session->getEventQueue();
}

}
