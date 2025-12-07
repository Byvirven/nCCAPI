#include "nccapi/exchanges/bybit.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"

#include "rapidjson/document.h"

namespace nccapi {

class Bybit::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        std::vector<std::string> categories = {"spot", "linear", "inverse", "option"};

        for (const auto& cat : categories) {
            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bybit", "", "");
            request.appendParam({
                {CCAPI_HTTP_PATH, "/v5/market/instruments-info"},
                {CCAPI_HTTP_METHOD, "GET"},
                {"category", cat}
            });

            session->sendRequest(request);

            auto start = std::chrono::steady_clock::now();
            bool received = false;
            while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
                std::vector<ccapi::Event> events = session->getEventQueue().purge();
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            if (message.getType() == ccapi::Message::Type::GENERIC_PUBLIC_REQUEST) {
                                for (const auto& element : message.getElementList()) {
                                    if (element.has(CCAPI_HTTP_BODY)) {
                                        std::string json_str = element.getValue(CCAPI_HTTP_BODY);
                                        rapidjson::Document doc;
                                        doc.Parse(json_str.c_str());
                                        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("result")) {
                                            const auto& result = doc["result"];
                                            if (result.HasMember("list") && result["list"].IsArray()) {
                                                for (const auto& item : result["list"].GetArray()) {
                                                    Instrument instrument;
                                                    instrument.id = item["symbol"].GetString();
                                                    instrument.base = item["baseCoin"].GetString();
                                                    instrument.quote = item["quoteCoin"].GetString();

                                                    if (item.HasMember("priceFilter") && item["priceFilter"].HasMember("tickSize")) {
                                                        instrument.tick_size = std::stod(item["priceFilter"]["tickSize"].GetString());
                                                    }
                                                    if (item.HasMember("lotSizeFilter") && item["lotSizeFilter"].HasMember("qtyStep")) {
                                                        instrument.step_size = std::stod(item["lotSizeFilter"]["qtyStep"].GetString());
                                                    }
                                                    if (item.HasMember("lotSizeFilter") && item["lotSizeFilter"].HasMember("minOrderQty")) {
                                                        instrument.min_size = std::stod(item["lotSizeFilter"]["minOrderQty"].GetString());
                                                    }

                                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                                    } else {
                                                        instrument.symbol = instrument.id;
                                                    }

                                                    if (cat == "spot") instrument.type = "spot";
                                                    else if (cat == "linear") instrument.type = "future"; // USDT Perpetual
                                                    else if (cat == "inverse") instrument.type = "future"; // Inverse Perpetual/Future
                                                    else if (cat == "option") instrument.type = "option";

                                                    if (item.HasMember("status")) {
                                                        instrument.active = (std::string(item["status"].GetString()) == "Trading");
                                                    }

                                                    instruments.push_back(instrument);
                                                }
                                                received = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if(received) break;
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
        // Bybit V5 uses GET_HISTORICAL_CANDLESTICKS -> /v5/market/kline
        ccapi::Request request(ccapi::Request::Operation::GET_HISTORICAL_CANDLESTICKS, "bybit", instrument_name);

        request.appendParam({
            {CCAPI_CANDLESTICK_INTERVAL_SECONDS, std::to_string(timeframeToSeconds(timeframe))},
            {CCAPI_START_TIME_SECONDS, std::to_string(from_date / 1000)},
            {CCAPI_END_TIME_SECONDS, std::to_string(to_date / 1000)},
            {CCAPI_LIMIT, "1000"}
        });

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GET_HISTORICAL_CANDLESTICKS ||
                            message.getType() == ccapi::Message::Type::MARKET_DATA_EVENTS_CANDLESTICK) {
                            for (const auto& element : message.getElementList()) {
                                Candle candle;
                                // Timestamp is in message.getTime() for native events
                                candle.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    message.getTime().time_since_epoch()).count();

                                candle.open = std::stod(element.getValue(CCAPI_OPEN_PRICE));
                                candle.high = std::stod(element.getValue(CCAPI_HIGH_PRICE));
                                candle.low = std::stod(element.getValue(CCAPI_LOW_PRICE));
                                candle.close = std::stod(element.getValue(CCAPI_CLOSE_PRICE));
                                candle.volume = std::stod(element.getValue(CCAPI_VOLUME));

                                candles.push_back(candle);
                            }
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            return candles;
                        }
                    }
                }
            }
            if (!candles.empty()) {
                std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                    return a.timestamp < b.timestamp;
                });
                return candles;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return candles;
    }

    int timeframeToSeconds(const std::string& timeframe) {
        if (timeframe == "1m") return 60;
        if (timeframe == "3m") return 180;
        if (timeframe == "5m") return 300;
        if (timeframe == "15m") return 900;
        if (timeframe == "30m") return 1800;
        if (timeframe == "1h") return 3600;
        if (timeframe == "2h") return 7200;
        if (timeframe == "4h") return 14400;
        if (timeframe == "6h") return 21600;
        if (timeframe == "12h") return 43200;
        if (timeframe == "1d") return 86400;
        if (timeframe == "1w") return 604800;
        if (timeframe == "1M") return 2592000;
        return 60;
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
