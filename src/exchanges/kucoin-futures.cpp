#include "nccapi/exchanges/kucoin-futures.hpp"
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

class KucoinFutures::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "kucoin-futures");

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

                                if(element.has(CCAPI_CONTRACT_SIZE)) {
                                     std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                     if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                }

                                if (!instrument.base.empty() && !instrument.quote.empty()) {
                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                } else {
                                    instrument.symbol = instrument.id;
                                }
                                instrument.type = "future"; // Perpetual or Future

                                if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                    instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "Open");
                                }

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
        const int limit = 200; // Kucoin Futures limit seems lower? 200? Or chunking needed?
        // But test returned OLDEST 200. So forward pagination works.
        // Let's use 200 chunk size.
        int max_loops = 100;

        // Kucoin Futures granularity: 1, 5, 15, 30, 60, 120, 240, 480, 720, 1440, 10080
        std::string granularity = "1";
        int64_t interval_ms = 60000;
        if (timeframe == "1m") { granularity = "1"; interval_ms = 60000; }
        else if (timeframe == "5m") { granularity = "5"; interval_ms = 300000; }
        else if (timeframe == "15m") { granularity = "15"; interval_ms = 900000; }
        else if (timeframe == "30m") { granularity = "30"; interval_ms = 1800000; }
        else if (timeframe == "1h") { granularity = "60"; interval_ms = 3600000; }
        else if (timeframe == "2h") { granularity = "120"; interval_ms = 7200000; }
        else if (timeframe == "4h") { granularity = "240"; interval_ms = 14400000; }
        else if (timeframe == "8h") { granularity = "480"; interval_ms = 28800000; }
        else if (timeframe == "12h") { granularity = "720"; interval_ms = 43200000; }
        else if (timeframe == "1d") { granularity = "1440"; interval_ms = 86400000; }
        else if (timeframe == "1w") { granularity = "10080"; interval_ms = 604800000; }

        while (current_from < to_date) {
            // Note: Futures API uses /api/v1/kline/query
            // Parameters: symbol, granularity (minutes), from, to (ms)
            // Kucoin Spot uses seconds, Futures uses ms.
            // Wait, documentation says "from: Start time (milisecond)".

            int64_t chunk_end = current_from + (limit * interval_ms);
            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "kucoin-futures", "", "");

            std::string query = "symbol=" + instrument_name + "&granularity=" + granularity;
            query += "&from=" + std::to_string(current_from);
            query += "&to=" + std::to_string(chunk_end);

            request.appendParam({
                {CCAPI_HTTP_PATH, "/api/v1/kline/query"},
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

                                        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data") && doc["data"].IsArray()) {
                                            for (const auto& kline : doc["data"].GetArray()) {
                                                if (kline.IsArray() && kline.Size() >= 6) {
                                                    Candle candle;
                                                    // [ time(ms), open, high, low, close, volume ]
                                                    if (kline[0].IsInt64()) candle.timestamp = kline[0].GetInt64();
                                                    else if (kline[0].IsDouble()) candle.timestamp = (int64_t)kline[0].GetDouble();

                                                    auto getVal = [](const rapidjson::Value& v) {
                                                        if(v.IsDouble()) return v.GetDouble();
                                                        if(v.IsString()) return std::stod(v.GetString());
                                                        return 0.0;
                                                    };

                                                    candle.open = getVal(kline[1]);
                                                    candle.high = getVal(kline[2]);
                                                    candle.low = getVal(kline[3]);
                                                    candle.close = getVal(kline[4]);
                                                    candle.volume = getVal(kline[5]);

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] Kucoin Futures Error: " << message.toString() << std::endl;
                                success = true;
                            }
                        }
                    }
                }
                if (success) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (batch_candles.empty()) {
                current_from = chunk_end;
            } else {
                 std::sort(batch_candles.begin(), batch_candles.end(), [](const Candle& a, const Candle& b) {
                    return a.timestamp < b.timestamp;
                });

                all_candles.insert(all_candles.end(), batch_candles.begin(), batch_candles.end());

                current_from = chunk_end;
            }

            if (--max_loops <= 0) break;
            if (current_from >= to_date) break;

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
                     if (to_date > 0 && c.timestamp > to_date) return true;
                     if (from_date > 0 && c.timestamp < from_date) return true;
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

KucoinFutures::KucoinFutures(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
KucoinFutures::~KucoinFutures() = default;

std::vector<Instrument> KucoinFutures::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> KucoinFutures::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
