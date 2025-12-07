#include "nccapi/exchanges/cryptocom.hpp"
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

class Cryptocom::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "cryptocom");

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

        // Crypto.com Generic Request
        // Endpoint: /v2/public/get-candlestick
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "cryptocom", "", "");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/v2/public/get-candlestick"},
            {CCAPI_HTTP_METHOD, "GET"}
        });

        // Timeframe: 1m, 5m, 15m, 30m, 1h, 4h, 6h, 12h, 1D, 7D, 14D, 1M
        std::string tf = "1m";
        if (timeframe == "1m") tf = "1m";
        else if (timeframe == "5m") tf = "5m";
        else if (timeframe == "15m") tf = "15m";
        else if (timeframe == "30m") tf = "30m";
        else if (timeframe == "1h") tf = "1h";
        else if (timeframe == "4h") tf = "4h";
        else if (timeframe == "6h") tf = "6h";
        else if (timeframe == "12h") tf = "12h";
        else if (timeframe == "1d") tf = "1D";
        else if (timeframe == "1w") tf = "7D";
        else if (timeframe == "1M") tf = "1M";
        else tf = "1m";

        request.appendParam({
            {"instrument_name", instrument_name},
            {"timeframe", tf}
        });

        // Note: Crypto.com v2 public API get-candlestick returns max 300? No pagination params?
        // Actually it seems it does not support pagination officially in public endpoint documentation.
        // It just returns latest.
        // If from_date is used, we can't pass it?
        // Some docs mention 'start_time' and 'end_time' but maybe exchange specific or private?
        // Trying 'start_ts' / 'end_ts'?
        // Docs say no params for time.
        // So we get what we get (latest 300).
        // Sorting and filtering will happen below.

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GENERIC_PUBLIC_REQUEST) {
                            for (const auto& element : message.getElementList()) {
                                std::string json_content = element.getValue(CCAPI_HTTP_BODY);
                                rapidjson::Document doc;
                                doc.Parse(json_content.c_str());

                                if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("result") && doc["result"].IsObject()) {
                                    const auto& result = doc["result"];
                                    if (result.HasMember("data") && result["data"].IsArray()) {
                                        for (const auto& kline : result["data"].GetArray()) {
                                            if (kline.IsObject()) {
                                                Candle candle;
                                                // {"t": 1600000000000, "o": ..., "h": ..., "l": ..., "c": ..., "v": ...}
                                                if (kline.HasMember("t") && kline["t"].IsInt64()) candle.timestamp = kline["t"].GetInt64();
                                                if (kline.HasMember("o") && kline["o"].IsNumber()) candle.open = kline["o"].GetDouble();
                                                if (kline.HasMember("h") && kline["h"].IsNumber()) candle.high = kline["h"].GetDouble();
                                                if (kline.HasMember("l") && kline["l"].IsNumber()) candle.low = kline["l"].GetDouble();
                                                if (kline.HasMember("c") && kline["c"].IsNumber()) candle.close = kline["c"].GetDouble();
                                                if (kline.HasMember("v") && kline["v"].IsNumber()) candle.volume = kline["v"].GetDouble();

                                                candles.push_back(candle);
                                            }
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
            if (!candles.empty()) {
                if (from_date > 0 || to_date > 0) {
                    candles.erase(std::remove_if(candles.begin(), candles.end(), [from_date, to_date](const Candle& c) {
                        if (from_date > 0 && c.timestamp < from_date) return true;
                        if (to_date > 0 && c.timestamp > to_date) return true;
                        return false;
                    }), candles.end());
                }
                std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                    return a.timestamp < b.timestamp;
                });
                return candles;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return candles;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

Cryptocom::Cryptocom(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Cryptocom::~Cryptocom() = default;

std::vector<Instrument> Cryptocom::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Cryptocom::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
