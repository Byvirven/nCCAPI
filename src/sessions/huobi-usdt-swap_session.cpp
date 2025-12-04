#include "nccapi/sessions/huobi-usdt-swap_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

HuobiUsdtSwapSession::HuobiUsdtSwapSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

HuobiUsdtSwapSession::~HuobiUsdtSwapSession() {
    delete session;
}

void HuobiUsdtSwapSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void HuobiUsdtSwapSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& HuobiUsdtSwapSession::getEventQueue() {
    return session->getEventQueue();
}

}
