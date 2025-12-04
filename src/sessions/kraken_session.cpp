#include "nccapi/sessions/kraken_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_KRAKEN
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

KrakenSession::KrakenSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

KrakenSession::~KrakenSession() {
    delete session;
}

void KrakenSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void KrakenSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& KrakenSession::getEventQueue() {
    return session->getEventQueue();
}

}
