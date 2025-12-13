#include "nccapi/exchanges/cryptocom.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

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
        std::vector<Candle> all_candles;

        int64_t current_from = from_date;
        const int limit = 300;
        int max_loops = 100;

        // Crypto.com generic path: /v2/public/get-candlestick
        std::string period = "1m";
        int64_t interval_ms = 60000;
        if (timeframe == "1m") { period = "1m"; interval_ms = 60000; }
        else if (timeframe == "5m") { period = "5m"; interval_ms = 300000; }
        else if (timeframe == "15m") { period = "15m"; interval_ms = 900000; }
        else if (timeframe == "30m") { period = "30m"; interval_ms = 1800000; }
        else if (timeframe == "1h") { period = "1h"; interval_ms = 3600000; }
        else if (timeframe == "4h") { period = "4h"; interval_ms = 14400000; }
        else if (timeframe == "6h") { period = "6h"; interval_ms = 21600000; }
        else if (timeframe == "12h") { period = "12h"; interval_ms = 43200000; }
        else if (timeframe == "1d") { period = "1D"; interval_ms = 86400000; }
        else if (timeframe == "1w") { period = "7D"; interval_ms = 604800000; }
        else if (timeframe == "2w") { period = "14D"; interval_ms = 1209600000; }
        else if (timeframe == "1M") { period = "1M"; interval_ms = 2592000000; }

        while (current_from < to_date) {
            int64_t chunk_end = current_from + (limit * interval_ms);
            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "cryptocom", "", "");

            std::string query = "instrument_name=" + instrument_name + "&timeframe=" + period;
            // query += "&count=" + std::to_string(limit); // Optional if window is small enough
            query += "&start_ts=" + std::to_string(current_from);
            query += "&end_ts=" + std::to_string(chunk_end);

            request.appendParam({
                {CCAPI_HTTP_PATH, "/v2/public/get-candlestick"},
                {CCAPI_HTTP_METHOD, "GET"},
                {CCAPI_HTTP_QUERY_STRING, query}
            });

            session->sendRequest(request);

            std::vector<Candle> batch_candles;
            bool success = false;

            auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
                std::vector<ccapi::Event> events = session->getEventQueue().purge();
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            if (message.getType() == ccapi::Message::Type::GENERIC_PUBLIC_REQUEST) {
                                for (const auto& element : message.getElementList()) {
                                    if (element.has(CCAPI_HTTP_BODY)) {
                                        std::string json_content = element.getValue(CCAPI_HTTP_BODY);
                                        rapidjson::Document doc;
                                        doc.Parse(json_content.c_str());

                                        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("result") && doc["result"].HasMember("data")) {
                                            const auto& data = doc["result"]["data"];
                                            if (data.IsArray()) {
                                                for (const auto& item : data.GetArray()) {
                                                    if (item.IsObject()) {
                                                        Candle candle;
                                                        if (item.HasMember("t")) candle.timestamp = item["t"].GetInt64();

                                                        auto getVal = [](const rapidjson::Value& v) -> double {
                                                            if(v.IsString()) return std::stod(v.GetString());
                                                            if(v.IsDouble()) return v.GetDouble();
                                                            if(v.IsInt64()) return (double)v.GetInt64();
                                                            return 0.0;
                                                        };

                                                        if (item.HasMember("o")) candle.open = getVal(item["o"]);
                                                        if (item.HasMember("h")) candle.high = getVal(item["h"]);
                                                        if (item.HasMember("l")) candle.low = getVal(item["l"]);
                                                        if (item.HasMember("c")) candle.close = getVal(item["c"]);
                                                        if (item.HasMember("v")) candle.volume = getVal(item["v"]);

                                                        batch_candles.push_back(candle);
                                                    }
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                success = true;
                            }
                        }
                    }
                }
                if (success) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (batch_candles.empty()) {
                // If empty, force move forward to avoid infinite loop
                current_from = chunk_end;
            } else {
                std::sort(batch_candles.begin(), batch_candles.end(), [](const Candle& a, const Candle& b) {
                    return a.timestamp < b.timestamp;
                });

                all_candles.insert(all_candles.end(), batch_candles.begin(), batch_candles.end());

                // Move to end of this chunk
                current_from = chunk_end;
            }

            if (--max_loops <= 0) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!all_candles.empty()) {
             std::sort(all_candles.begin(), all_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });
            auto last = std::unique(all_candles.begin(), all_candles.end(), [](const Candle& a, const Candle& b){
                return a.timestamp == b.timestamp;
            });
            all_candles.erase(last, all_candles.end());

            if (from_date > 0 || to_date > 0) {
                auto it = std::remove_if(all_candles.begin(), all_candles.end(), [from_date, to_date](const Candle& c) {
                    if (from_date > 0 && c.timestamp < from_date) return true;
                    if (to_date > 0 && c.timestamp > to_date) return true;
                    return false;
                });
                all_candles.erase(it, all_candles.end());
            }
        }

        return all_candles;
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
