#include "nccapi/sessions/unified_session.hpp"

// Define services
#define CCAPI_ENABLE_SERVICE_MARKET_DATA
#define CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT

// Define ALL exchanges
#define CCAPI_ENABLE_EXCHANGE_ASCENDEX
#define CCAPI_ENABLE_EXCHANGE_BINANCE
#define CCAPI_ENABLE_EXCHANGE_BINANCE_US
#define CCAPI_ENABLE_EXCHANGE_BINANCE_COIN_FUTURES
#define CCAPI_ENABLE_EXCHANGE_BINANCE_USDS_FUTURES
#define CCAPI_ENABLE_EXCHANGE_BITFINEX
#define CCAPI_ENABLE_EXCHANGE_BITGET
#define CCAPI_ENABLE_EXCHANGE_BITGET_FUTURES
#define CCAPI_ENABLE_EXCHANGE_BITMART
#define CCAPI_ENABLE_EXCHANGE_BITMEX
#define CCAPI_ENABLE_EXCHANGE_BITSTAMP
#define CCAPI_ENABLE_EXCHANGE_BYBIT
#define CCAPI_ENABLE_EXCHANGE_COINBASE
#define CCAPI_ENABLE_EXCHANGE_CRYPTOCOM
#define CCAPI_ENABLE_EXCHANGE_DERIBIT
// #define CCAPI_ENABLE_EXCHANGE_ERISX // Disabled due to migration to Cboe Digital
#define CCAPI_ENABLE_EXCHANGE_GATEIO
#define CCAPI_ENABLE_EXCHANGE_GATEIO_PERPETUAL_FUTURES
#define CCAPI_ENABLE_EXCHANGE_GEMINI
#define CCAPI_ENABLE_EXCHANGE_HUOBI
#define CCAPI_ENABLE_EXCHANGE_HUOBI_COIN_SWAP
#define CCAPI_ENABLE_EXCHANGE_HUOBI_USDT_SWAP
#define CCAPI_ENABLE_EXCHANGE_KRAKEN
#define CCAPI_ENABLE_EXCHANGE_KRAKEN_FUTURES
#define CCAPI_ENABLE_EXCHANGE_KUCOIN
#define CCAPI_ENABLE_EXCHANGE_KUCOIN_FUTURES
#define CCAPI_ENABLE_EXCHANGE_MEXC
#define CCAPI_ENABLE_EXCHANGE_MEXC_FUTURES
#define CCAPI_ENABLE_EXCHANGE_OKX
#define CCAPI_ENABLE_EXCHANGE_WHITEBIT

#include "ccapi_cpp/ccapi_session.h"

namespace nccapi {

UnifiedSession::UnifiedSession(const ccapi::SessionOptions& options, const ccapi::SessionConfigs& configs, ccapi::EventHandler* eventHandler) {
    session = new ccapi::Session(options, configs, eventHandler);
}

UnifiedSession::~UnifiedSession() {
    if (session) {
        session->stop();
        delete session;
    }
}

void UnifiedSession::sendRequest(ccapi::Request& request) {
    session->sendRequest(request);
}

void UnifiedSession::stop() {
    session->stop();
}

ccapi::Queue<ccapi::Event>& UnifiedSession::getEventQueue() {
    return session->getEventQueue();
}

}
