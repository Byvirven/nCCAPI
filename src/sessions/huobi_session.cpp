#include "nccapi/sessions/huobi_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_HUOBI
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

HuobiSession::HuobiSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

HuobiSession::~HuobiSession() {
    delete session;
}

void HuobiSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void HuobiSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& HuobiSession::getEventQueue() {
    return session->getEventQueue();
}

}
