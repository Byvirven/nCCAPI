#include "nccapi/sessions/gemini_session.hpp"

#define CCAPI_ENABLE_SERVICE_MARKET_DATA
// Macro defined in CMakeLists.txt via property, but let's document it
// #define CCAPI_ENABLE_EXCHANGE_GEMINI
#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

GeminiSession::GeminiSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

GeminiSession::~GeminiSession() {
    delete session;
}

void GeminiSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void GeminiSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& GeminiSession::getEventQueue() {
    return session->getEventQueue();
}

}
