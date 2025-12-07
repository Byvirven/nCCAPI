#include "nccapi/exchanges/ascendex.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "rapidjson/document.h"

namespace nccapi {

class Ascendex::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "ascendex");

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

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "ascendex", "", "");

        // AscendEX timeframe: 1, 5, 15, 30, 60, 120, 240, 360, 720, 1d, 1m, 1w (string)
        std::string interval = "1";
        if (timeframe == "1m") interval = "1";
        else if (timeframe == "5m") interval = "5";
        else if (timeframe == "15m") interval = "15";
        else if (timeframe == "30m") interval = "30";
        else if (timeframe == "1h") interval = "60";
        else if (timeframe == "2h") interval = "120";
        else if (timeframe == "4h") interval = "240";
        else if (timeframe == "6h") interval = "360";
        else if (timeframe == "12h") interval = "720";
        else if (timeframe == "1d") interval = "1d";
        else if (timeframe == "1w") interval = "1w";
        else if (timeframe == "1M") interval = "1m";
        else interval = "1";

        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/pro/v1/barhist"},
            {CCAPI_HTTP_METHOD, "GET"},
            {"symbol", instrument_name},
            {"interval", interval},
            {"from", std::to_string(from_date)}, // ms
            {"to", std::to_string(to_date)},
            {"n", "500"} // Limit
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

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data") && doc["data"].IsArray()) {
                                        for (const auto& item : doc["data"].GetArray()) {
                                            if (item.IsObject()) {
                                                Candle candle;
                                                if (item.HasMember("data") && item["data"].IsObject()) {
                                                    const auto& d = item["data"];
                                                    if (d.HasMember("ts")) candle.timestamp = d["ts"].GetInt64();
                                                    if (d.HasMember("o")) candle.open = std::stod(d["o"].GetString());
                                                    if (d.HasMember("h")) candle.high = std::stod(d["h"].GetString());
                                                    if (d.HasMember("l")) candle.low = std::stod(d["l"].GetString());
                                                    if (d.HasMember("c")) candle.close = std::stod(d["c"].GetString());
                                                    if (d.HasMember("v")) candle.volume = std::stod(d["v"].GetString());
                                                    candles.push_back(candle);
                                                } else {
                                                    // Direct object? AscendEX response: "data": [ { "m": "bar", "s": "BTC/USDT", "data": { "i": "1", "ts": 157..., "o":... } } ]
                                                    // NO, wait.
                                                    // Documentation: GET /api/pro/v1/barhist
                                                    // Response: { "code": 0, "data": [ { "m": "bar", "s": "BTC/USDT", "data": { "i": "1", "ts": 1575503040000, "o": "7538.99", "c": "7538.99", "h": "7538.99", "l": "7538.99", "v": "0" } }, ... ] }
                                                    // So item has "data" field which contains the OHLCV.
                                                    if (item.HasMember("data") && item["data"].IsObject()) {
                                                        const auto& d = item["data"];
                                                        if (d.HasMember("ts")) candle.timestamp = d["ts"].GetInt64();
                                                        if (d.HasMember("o")) candle.open = std::stod(d["o"].GetString());
                                                        if (d.HasMember("h")) candle.high = std::stod(d["h"].GetString());
                                                        if (d.HasMember("l")) candle.low = std::stod(d["l"].GetString());
                                                        if (d.HasMember("c")) candle.close = std::stod(d["c"].GetString());
                                                        if (d.HasMember("v")) candle.volume = std::stod(d["v"].GetString());
                                                        candles.push_back(candle);
                                                    }
                                                }
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

private:
    std::shared_ptr<UnifiedSession> session;
};

Ascendex::Ascendex(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Ascendex::~Ascendex() = default;

std::vector<Instrument> Ascendex::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Ascendex::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
