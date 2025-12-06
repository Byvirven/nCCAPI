#include "nccapi/exchanges/bybit.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"

namespace nccapi {

class Bybit::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        std::vector<std::string> categories = {"spot", "linear", "inverse", "option"};

        for (const auto& category : categories) {
            ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bybit");
            // CCAPI requires "category" param for Bybit
            request.appendParam({
                {CCAPI_INSTRUMENT_TYPE, category}
            });

            session->sendRequest(request);

            auto start = std::chrono::steady_clock::now();
            bool received = false;
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
                                    instrument.quote = element.getValue(CCAPI_QUOTE_ASSET);

                                    std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                    if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                    std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                    if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                    std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                    if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }

                                    instrument.type = category;
                                    instrument.active = true;

                                    instruments.push_back(instrument);
                                }
                                received = true;
                            }
                        }
                    }
                }
                if (received) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return instruments;
    }

    std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                               const std::string& timeframe,
                                               int64_t from_date,
                                               int64_t to_date) {
        std::vector<Candle> candles;
        ccapi::Request request(ccapi::Request::Operation::GET_RECENT_CANDLESTICKS, "bybit", instrument_name);

        // Bybit V5: 1, 3, 5, 15, 30, 60, 120, 240, 360, 720, D, M, W
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
            {CCAPI_END_TIME_SECONDS, std::to_string(to_date / 1000)},
            {"limit", "1000"}
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
};

Bybit::Bybit(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Bybit::~Bybit() = default;

std::vector<Instrument> Bybit::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Bybit::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
