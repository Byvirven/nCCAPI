#include "nccapi/exchanges/bitstamp.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "rapidjson/document.h"

namespace nccapi {

class Bitstamp::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bitstamp");

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

                                std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN); // Check if available
                                if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                if (!instrument.base.empty() && !instrument.quote.empty()) {
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

        // Use GENERIC_PUBLIC_REQUEST for Bitstamp
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitstamp", "", "GET_OHLC");

        // Bitstamp: step in seconds
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
        else if (timeframe == "3d") interval_seconds = 259200;
        else interval_seconds = 60;

        // Path: /api/v2/ohlc/{pair}/
        std::string path = "/api/v2/ohlc/" + instrument_name + "/";

        request.appendParam({
            {CCAPI_HTTP_PATH, path},
            {CCAPI_HTTP_METHOD, "GET"},
            {"step", std::to_string(interval_seconds)},
            {"start", std::to_string(from_date / 1000)},
            {"end", std::to_string(to_date / 1000)},
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

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data") && doc["data"].HasMember("ohlc")) {
                                        const auto& ohlc = doc["data"]["ohlc"];
                                        if (ohlc.IsArray()) {
                                            for (const auto& item : ohlc.GetArray()) {
                                                // {"high": "...", "timestamp": "...", "volume": "...", "low": "...", "close": "...", "open": "..."}
                                                if (item.IsObject()) {
                                                    Candle candle;
                                                    if (item.HasMember("timestamp")) {
                                                        candle.timestamp = (int64_t)std::stoll(item["timestamp"].GetString()) * 1000;
                                                    }
                                                    if (item.HasMember("open")) candle.open = std::stod(item["open"].GetString());
                                                    if (item.HasMember("high")) candle.high = std::stod(item["high"].GetString());
                                                    if (item.HasMember("low")) candle.low = std::stod(item["low"].GetString());
                                                    if (item.HasMember("close")) candle.close = std::stod(item["close"].GetString());
                                                    if (item.HasMember("volume")) candle.volume = std::stod(item["volume"].GetString());

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

private:
    std::shared_ptr<UnifiedSession> session;
};

Bitstamp::Bitstamp(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Bitstamp::~Bitstamp() = default;

std::vector<Instrument> Bitstamp::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Bitstamp::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
