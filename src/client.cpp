#include "nccapi/client.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

#include "nccapi/sessions/unified_session.hpp"

// Include all exchanges
#include "nccapi/exchanges/ascendex.hpp"
#include "nccapi/exchanges/binance.hpp"
#include "nccapi/exchanges/binance-coin-futures.hpp"
#include "nccapi/exchanges/binance-us.hpp"
#include "nccapi/exchanges/binance-usds-futures.hpp"
#include "nccapi/exchanges/bitfinex.hpp"
#include "nccapi/exchanges/bitget.hpp"
#include "nccapi/exchanges/bitget-futures.hpp"
#include "nccapi/exchanges/bitmart.hpp"
#include "nccapi/exchanges/bitmex.hpp"
#include "nccapi/exchanges/bitstamp.hpp"
#include "nccapi/exchanges/bybit.hpp"
#include "nccapi/exchanges/coinbase.hpp"
#include "nccapi/exchanges/cryptocom.hpp"
#include "nccapi/exchanges/deribit.hpp"
#include "nccapi/exchanges/erisx.hpp"
#include "nccapi/exchanges/gateio.hpp"
#include "nccapi/exchanges/gateio-perpetual-futures.hpp"
#include "nccapi/exchanges/gemini.hpp"
#include "nccapi/exchanges/huobi.hpp"
#include "nccapi/exchanges/huobi-coin-swap.hpp"
#include "nccapi/exchanges/huobi-usdt-swap.hpp"
#include "nccapi/exchanges/kraken.hpp"
#include "nccapi/exchanges/kraken-futures.hpp"
#include "nccapi/exchanges/kucoin.hpp"
#include "nccapi/exchanges/kucoin-futures.hpp"
#include "nccapi/exchanges/mexc.hpp"
#include "nccapi/exchanges/mexc-futures.hpp"
#include "nccapi/exchanges/okx.hpp"
#include "nccapi/exchanges/whitebit.hpp"

namespace nccapi {

Client::Client() {
    // Instantiate Unified Session
    ccapi::SessionOptions options;
    ccapi::SessionConfigs configs;
    auto unifiedSession = std::make_shared<UnifiedSession>(options, configs);

    // Register all exchanges, passing the unified session
    exchanges_["ascendex"] = std::make_shared<Ascendex>(unifiedSession);
    exchanges_["binance"] = std::make_shared<Binance>(unifiedSession);
    exchanges_["binance-coin-futures"] = std::make_shared<BinanceCoinFutures>(unifiedSession);
    exchanges_["binance-us"] = std::make_shared<BinanceUs>(unifiedSession);
    exchanges_["binance-usds-futures"] = std::make_shared<BinanceUsdsFutures>(unifiedSession);
    exchanges_["bitfinex"] = std::make_shared<Bitfinex>(unifiedSession);
    exchanges_["bitget"] = std::make_shared<Bitget>(unifiedSession);
    exchanges_["bitget-futures"] = std::make_shared<BitgetFutures>(unifiedSession);
    exchanges_["bitmart"] = std::make_shared<Bitmart>(unifiedSession);
    exchanges_["bitmex"] = std::make_shared<Bitmex>(unifiedSession);
    exchanges_["bitstamp"] = std::make_shared<Bitstamp>(unifiedSession);
    exchanges_["bybit"] = std::make_shared<Bybit>(unifiedSession);
    exchanges_["coinbase"] = std::make_shared<Coinbase>(unifiedSession);
    exchanges_["cryptocom"] = std::make_shared<Cryptocom>(unifiedSession);
    exchanges_["deribit"] = std::make_shared<Deribit>(unifiedSession);
    exchanges_["erisx"] = std::make_shared<Erisx>(unifiedSession);
    exchanges_["gateio"] = std::make_shared<Gateio>(unifiedSession);
    exchanges_["gateio-perpetual-futures"] = std::make_shared<GateioPerpetualFutures>(unifiedSession);
    exchanges_["gemini"] = std::make_shared<Gemini>(unifiedSession);
    exchanges_["huobi"] = std::make_shared<Huobi>(unifiedSession);
    exchanges_["huobi-coin-swap"] = std::make_shared<HuobiCoinSwap>(unifiedSession);
    exchanges_["huobi-usdt-swap"] = std::make_shared<HuobiUsdtSwap>(unifiedSession);
    exchanges_["kraken"] = std::make_shared<Kraken>(unifiedSession);
    exchanges_["kraken-futures"] = std::make_shared<KrakenFutures>(unifiedSession);
    exchanges_["kucoin"] = std::make_shared<Kucoin>(unifiedSession);
    exchanges_["kucoin-futures"] = std::make_shared<KucoinFutures>(unifiedSession);
    exchanges_["mexc"] = std::make_shared<Mexc>(unifiedSession);
    exchanges_["mexc-futures"] = std::make_shared<MexcFutures>(unifiedSession);
    exchanges_["okx"] = std::make_shared<Okx>(unifiedSession);
    exchanges_["whitebit"] = std::make_shared<Whitebit>(unifiedSession);
}

Client::~Client() = default;

std::vector<std::string> Client::get_supported_exchanges() const {
    std::vector<std::string> names;
    for (const auto& pair : exchanges_) {
        names.push_back(pair.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::shared_ptr<Exchange> Client::get_exchange(const std::string& exchange_name) {
    auto it = exchanges_.find(exchange_name);
    if (it != exchanges_.end()) {
        return it->second;
    }

    throw std::runtime_error("Exchange not supported: " + exchange_name);
}

std::vector<Instrument> Client::get_pairs(const std::string& exchange_name) {
    auto exchange = get_exchange(exchange_name);
    return exchange->get_instruments();
}

} // namespace nccapi
