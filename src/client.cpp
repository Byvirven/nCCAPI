#include "nccapi/client.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

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
// #include "nccapi/exchanges/erisx.hpp" // Disabled
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

namespace {
    int64_t timeframe_to_ms(const std::string& timeframe) {
        if (timeframe.empty()) return 0;
        size_t unit_pos = 0;
        while (unit_pos < timeframe.length() && std::isdigit(timeframe[unit_pos])) {
            unit_pos++;
        }
        if (unit_pos == 0) return 0; // No digits

        int64_t value = std::stoll(timeframe.substr(0, unit_pos));
        std::string unit = timeframe.substr(unit_pos);

        if (unit == "s") return value * 1000;
        if (unit == "m") return value * 60 * 1000;
        if (unit == "h") return value * 3600 * 1000;
        if (unit == "d") return value * 86400 * 1000;
        if (unit == "w") return value * 604800 * 1000;
        if (unit == "M") return value * 2592000000LL; // 30 days approx
        if (unit == "y") return value * 31536000000LL; // 365 days

        return 0;
    }
}

Client::Client() {
    // Instantiate Unified Session
    ccapi::SessionOptions options;
    options.httpRequestTimeoutMilliseconds = 30000; // Increase default timeout to 30s
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
    // exchanges_["erisx"] = std::make_shared<Erisx>(unifiedSession); // Disabled
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

std::vector<Candle> Client::get_historical_candles(const std::string& exchange_name,
                                                   const std::string& instrument_name,
                                                   const std::string& timeframe,
                                                   int64_t from_date,
                                                   int64_t to_date) {
    auto exchange = get_exchange(exchange_name);

    // Set default logic if needed, although generic default args handle it.
    // However, if the user passes 0, we can interpret it as defaults here if specific logic requires it.
    int64_t actual_to_date = to_date;
    if (actual_to_date <= 0) {
        actual_to_date = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    int64_t interval_ms = timeframe_to_ms(timeframe);

    // Align request from_date to interval boundary to ensure we get the full first candle
    int64_t aligned_from = from_date;
    if (interval_ms > 0 && aligned_from > 0) {
        aligned_from = (aligned_from / interval_ms) * interval_ms;
    }

    auto candles = exchange->get_historical_candles(instrument_name, timeframe, aligned_from, actual_to_date);

    // 1. Sort to ensure time order
    std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
        return a.timestamp < b.timestamp;
    });

    // 2. Filter / Truncate irrelevant candles (intelligent truncation)
    // Remove candles strictly outside the requested [from_date, to_date) range.
    // Note: We used aligned_from for the request to ensure we got the start bucket,
    // but we filter using the aligned_from as well to keep that start bucket if it covers the request.

    int64_t effective_from = aligned_from > 0 ? aligned_from : from_date;

    if (effective_from > 0 || actual_to_date > 0) {
        auto it = std::remove_if(candles.begin(), candles.end(), [effective_from, actual_to_date](const Candle& c) {
            if (effective_from > 0 && c.timestamp < effective_from) return true;
            if (actual_to_date > 0 && c.timestamp >= actual_to_date) return true;
            return false;
        });
        candles.erase(it, candles.end());
    }

    // Gap Filling Logic (Zero-Gap Data Policy)
    if (candles.empty()) return candles;

    if (interval_ms <= 0) return candles; // Cannot fill gaps if timeframe unknown

    std::vector<Candle> filled_candles;
    filled_candles.reserve(candles.size() * 2); // Pre-allocate some space

    filled_candles.push_back(candles[0]);

    for (size_t i = 1; i < candles.size(); ++i) {
        Candle prev = filled_candles.back(); // Copy by value to avoid reference invalidation on push_back
        const auto& curr = candles[i];
        int64_t diff = curr.timestamp - prev.timestamp;

        // Check for gap (allow small tolerance, e.g. 10ms of interval, for slight time skews)
        if (diff > interval_ms + 10) {
             int64_t next_ts = prev.timestamp + interval_ms;
             while (next_ts < curr.timestamp - (interval_ms / 2)) { // While we are not close enough to current
                 Candle gap_candle;
                 gap_candle.timestamp = next_ts;
                 gap_candle.open = prev.close;
                 gap_candle.high = prev.close;
                 gap_candle.low = prev.close;
                 gap_candle.close = prev.close;
                 gap_candle.volume = 0.0;
                 filled_candles.push_back(gap_candle);
                 next_ts += interval_ms;
             }
        }
        filled_candles.push_back(curr);
    }

    return filled_candles;
}

} // namespace nccapi
