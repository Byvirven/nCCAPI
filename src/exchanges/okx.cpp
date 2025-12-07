#include "nccapi/exchanges/okx.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"

namespace nccapi {

class Okx::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> all_instruments;
        std::vector<std::string> instTypes = {"SPOT", "SWAP", "FUTURES", "OPTION"};

        for (const auto& instType : instTypes) {
            std::vector<Instrument> batch = fetch_instruments_by_type(instType);
            all_instruments.insert(all_instruments.end(), batch.begin(), batch.end());
            // Small delay to avoid rate limits
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        return all_instruments;
    }

    std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                               const std::string& timeframe,
                                               int64_t from_date,
                                               int64_t to_date) {
        std::vector<Candle> candles;
        ccapi::Request request(ccapi::Request::Operation::GET_RECENT_CANDLESTICKS, "okx", instrument_name);

        // OKX intervals: 1m, 3m, 5m, 15m, 30m, 1H, 2H, 4H, 6H, 12H, 1D, 1W, 1M, 3M, 6M, 1Y

        int interval_seconds = 60;
        if (timeframe == "1m") interval_seconds = 60;
        else if (timeframe == "3m") interval_seconds = 180;
        else if (timeframe == "5m") interval_seconds = 300;
        else if (timeframe == "15m") interval_seconds = 900;
        else if (timeframe == "30m") interval_seconds = 1800;
        else if (timeframe == "1h") interval_seconds = 3600;
        else if (timeframe == "2h") interval_seconds = 7200;
        else if (timeframe == "4h") interval_seconds = 14400;
        else if (timeframe == "6h") interval_seconds = 21600;
        else if (timeframe == "12h") interval_seconds = 43200;
        else if (timeframe == "1d") interval_seconds = 86400;
        else if (timeframe == "1w") interval_seconds = 604800;
        else interval_seconds = 60;

        request.appendParam({
            {CCAPI_CANDLESTICK_INTERVAL_SECONDS, std::to_string(interval_seconds)},
            {CCAPI_START_TIME_SECONDS, std::to_string(from_date / 1000)},
            {CCAPI_END_TIME_SECONDS, std::to_string(to_date / 1000)}
        });

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GET_RECENT_CANDLESTICKS ||
                            message.getType() == ccapi::Message::Type::MARKET_DATA_EVENTS_CANDLESTICK) {
                            for (const auto& element : message.getElementList()) {
                                Candle candle;
                                std::string ts_str = element.getValue("TIMESTAMP");
                                if (!ts_str.empty()) {
                                    candle.timestamp = std::stoull(ts_str);
                                }
                                candle.open = std::stod(element.getValue(CCAPI_OPEN_PRICE));
                                candle.high = std::stod(element.getValue(CCAPI_HIGH_PRICE));
                                candle.low = std::stod(element.getValue(CCAPI_LOW_PRICE));
                                candle.close = std::stod(element.getValue(CCAPI_CLOSE_PRICE));
                                candle.volume = std::stod(element.getValue(CCAPI_VOLUME));

                                candles.push_back(candle);
                            }
                            std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                return a.timestamp < b.timestamp;
                            });
                            return candles;
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            return candles;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return candles;
    }

private:
    std::shared_ptr<UnifiedSession> session;

    std::vector<Instrument> fetch_instruments_by_type(const std::string& instType) {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "okx");
        request.appendParam({{"instType", instType}});

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GET_INSTRUMENTS) {
                            for (const auto& element : message.getElementList()) {
                                Instrument instrument;
                                instrument.id = element.getValue(CCAPI_INSTRUMENT);
                                instrument.base = element.getValue(CCAPI_BASE_ASSET);
                                instrument.quote = element.getValue(CCAPI_QUOTE_ASSET); // Note: For Futures/Options, might be empty or settlement currency

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                // Normalized Symbol
                                if (instType == "SPOT") {
                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }
                                    instrument.type = "spot";
                                } else {
                                    instrument.symbol = instrument.id; // Keep original ID for derivatives
                                    if (instType == "SWAP") instrument.type = "swap";
                                    else if (instType == "FUTURES") instrument.type = "future";
                                    else if (instType == "OPTION") instrument.type = "option";
                                }

                                // Status
                                if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                    instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "live");
                                }

                                // Derivative fields
                                if (element.has(CCAPI_CONTRACT_SIZE)) {
                                    std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                    if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                }
                                if (element.has(CCAPI_CONTRACT_MULTIPLIER)) {
                                    std::string val = element.getValue(CCAPI_CONTRACT_MULTIPLIER);
                                    if(!val.empty()) { try { instrument.contract_multiplier = std::stod(val); } catch(...) {} }
                                }
                                if (element.has(CCAPI_UNDERLYING_SYMBOL)) instrument.underlying = element.getValue(CCAPI_UNDERLYING_SYMBOL);
                                // Corrected field name
                                if (element.has(CCAPI_SETTLE_ASSET)) instrument.settle = element.getValue(CCAPI_SETTLE_ASSET);

                                // Store raw info
                                for (const auto& pair : element.getNameValueMap()) {
                                    instrument.info[std::string(pair.first)] = pair.second;
                                }

                                instruments.push_back(instrument);
                            }
                            return instruments;
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                             // std::cerr << "Okx Error (" << instType << "): " << message.getElementList()[0].getValue(CCAPI_ERROR_MESSAGE) << std::endl;
                             return instruments;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return instruments;
    }
};

Okx::Okx(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Okx::~Okx() = default;

std::vector<Instrument> Okx::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Okx::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
