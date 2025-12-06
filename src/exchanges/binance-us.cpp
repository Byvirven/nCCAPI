#include "nccapi/exchanges/binance-us.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"

// RapidJSON for manual parsing
#include "rapidjson/document.h"

namespace nccapi {

class BinanceUs::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;

        // Use GENERIC_PUBLIC_REQUEST to avoid CCAPI adding ?showPermissionSets=false (Error -1104)
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "binance-us", "", "GET_INSTRUMENTS");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v3/exchangeInfo"},
            {CCAPI_HTTP_METHOD, "GET"}
        });

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
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

                                    if (!doc.HasParseError() && doc.HasMember("symbols") && doc["symbols"].IsArray()) {
                                        for (const auto& s : doc["symbols"].GetArray()) {
                                            Instrument instrument;
                                            instrument.id = s["symbol"].GetString();
                                            instrument.base = s["baseAsset"].GetString();
                                            instrument.quote = s["quoteAsset"].GetString();

                                            // Status
                                            if (s.HasMember("status")) {
                                                instrument.active = (std::string(s["status"].GetString()) == "TRADING");
                                            }

                                            // Filters
                                            if (s.HasMember("filters") && s["filters"].IsArray()) {
                                                for (const auto& f : s["filters"].GetArray()) {
                                                    if (f.HasMember("filterType")) {
                                                        std::string type = f["filterType"].GetString();
                                                        if (type == "PRICE_FILTER") {
                                                            if (f.HasMember("tickSize")) instrument.tick_size = std::stod(f["tickSize"].GetString());
                                                        } else if (type == "LOT_SIZE") {
                                                            if (f.HasMember("stepSize")) instrument.step_size = std::stod(f["stepSize"].GetString());
                                                            if (f.HasMember("minQty")) instrument.min_size = std::stod(f["minQty"].GetString());
                                                        } else if (type == "NOTIONAL") {
                                                            if (f.HasMember("minNotional")) instrument.min_notional = std::stod(f["minNotional"].GetString());
                                                        }
                                                    }
                                                }
                                            }

                                            if (!instrument.base.empty() && !instrument.quote.empty()) {
                                                instrument.symbol = instrument.base + "/" + instrument.quote;
                                            } else {
                                                instrument.symbol = instrument.id;
                                            }
                                            instrument.type = "spot";
                                            instruments.push_back(instrument);
                                        }
                                        return instruments;
                                    }
                                }
                            }
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

        // Use GENERIC_PUBLIC_REQUEST for BinanceUS to ensure reliable execution
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "binance-us", "", "GET_KLINES");

        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v3/klines"},
            {CCAPI_HTTP_METHOD, "GET"},
            {"symbol", instrument_name},
            {"interval", timeframe}, // Binance uses "1m", "1h" directly
            {"startTime", std::to_string(from_date)}, // Binance uses ms
            {"endTime", std::to_string(to_date)},
            {"limit", "1000"}
        });

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
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

                                    if (!doc.HasParseError() && doc.IsArray()) {
                                        for (const auto& item : doc.GetArray()) {
                                            if (item.IsArray() && item.Size() >= 6) {
                                                Candle candle;
                                                candle.timestamp = item[0].GetInt64();
                                                candle.open = std::stod(item[1].GetString());
                                                candle.high = std::stod(item[2].GetString());
                                                candle.low = std::stod(item[3].GetString());
                                                candle.close = std::stod(item[4].GetString());
                                                candle.volume = std::stod(item[5].GetString());
                                                candles.push_back(candle);
                                            }
                                        }
                                        std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                            return a.timestamp < b.timestamp;
                                        });
                                        return candles;
                                    }
                                }
                            }
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
        if (timeframe == "8h") return 28800;
        if (timeframe == "12h") return 43200;
        if (timeframe == "1d") return 86400;
        if (timeframe == "3d") return 259200;
        if (timeframe == "1w") return 604800;
        if (timeframe == "1M") return 2592000;
        return 60;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

BinanceUs::BinanceUs(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
BinanceUs::~BinanceUs() = default;

std::vector<Instrument> BinanceUs::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> BinanceUs::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
