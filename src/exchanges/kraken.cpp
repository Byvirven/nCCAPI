#include "nccapi/exchanges/kraken.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"

namespace nccapi {

class Kraken::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "kraken");

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GET_INSTRUMENTS) {
                            for (const auto& element : message.getElementList()) {
                                Instrument instrument;
                                instrument.id = element.getValue(CCAPI_INSTRUMENT);
                                instrument.base = element.getValue(CCAPI_BASE_ASSET);
                                instrument.quote = element.getValue(CCAPI_QUOTE_ASSET);

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                if (element.has("WSNAME")) {
                                    // Kraken WSNAME is usually normalized "BASE/QUOTE"
                                    instrument.symbol = element.getValue("WSNAME");
                                } else if (!instrument.base.empty() && !instrument.quote.empty()) {
                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                } else {
                                    instrument.symbol = instrument.id;
                                }
                                instrument.type = "spot";

                                for (const auto& pair : element.getNameValueMap()) {
                                    instrument.info[std::string(pair.first)] = pair.second;
                                }

                                instruments.push_back(instrument);
                            }
                            return instruments;
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            return instruments;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return instruments;
    }

    std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                               const std::string& timeframe,
                                               int64_t from_date,
                                               int64_t to_date) {
        std::vector<Candle> candles;
        // Kraken uses GET_RECENT_CANDLESTICKS -> /0/public/OHLC
        ccapi::Request request(ccapi::Request::Operation::GET_RECENT_CANDLESTICKS, "kraken", instrument_name);

        // Kraken interval is in minutes (1, 5, 15, 30, 60, 240, 1440, 10080, 21600)
        int interval_minutes = 1;
        if (timeframe == "1m") interval_minutes = 1;
        else if (timeframe == "5m") interval_minutes = 5;
        else if (timeframe == "15m") interval_minutes = 15;
        else if (timeframe == "30m") interval_minutes = 30;
        else if (timeframe == "1h") interval_minutes = 60;
        else if (timeframe == "4h") interval_minutes = 240;
        else if (timeframe == "1d") interval_minutes = 1440;
        else if (timeframe == "1w") interval_minutes = 10080;
        else interval_minutes = 1;

        // Kraken takes "since" (seconds?) or ccapi handles it?
        // CCAPI maps CCAPI_START_TIME_SECONDS to "since".

        request.appendParam({
            {CCAPI_CANDLESTICK_INTERVAL_SECONDS, std::to_string(interval_minutes * 60)}, // CCAPI expects seconds for consistency
            {CCAPI_START_TIME_SECONDS, std::to_string(from_date / 1000)}
            // Kraken doesn't support explicit end time for OHLC, it returns since 'since'.
        });

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GET_RECENT_CANDLESTICKS) {
                            for (const auto& element : message.getElementList()) {
                                Candle candle;
                                std::string ts_str = element.getValue("TIMESTAMP"); // Raw string lookup
                                if (!ts_str.empty()) {
                                    candle.timestamp = std::stoull(ts_str);
                                }
                                candle.open = std::stod(element.getValue(CCAPI_OPEN_PRICE));
                                candle.high = std::stod(element.getValue(CCAPI_HIGH_PRICE));
                                candle.low = std::stod(element.getValue(CCAPI_LOW_PRICE));
                                candle.close = std::stod(element.getValue(CCAPI_CLOSE_PRICE));
                                candle.volume = std::stod(element.getValue(CCAPI_VOLUME));

                                // Filter by to_date if Kraken returns more
                                if (to_date > 0 && candle.timestamp > to_date) continue;

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
};

Kraken::Kraken(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Kraken::~Kraken() = default;

std::vector<Instrument> Kraken::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Kraken::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
