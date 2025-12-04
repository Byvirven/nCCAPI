#include "nccapi/sessions/huobi-coin-swap_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

HuobiCoinSwapSession::HuobiCoinSwapSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

HuobiCoinSwapSession::~HuobiCoinSwapSession() {
    delete session;
}

void HuobiCoinSwapSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void HuobiCoinSwapSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& HuobiCoinSwapSession::getEventQueue() {
    return session->getEventQueue();
}

}
