#include "nccapi/exchanges/mexc.hpp"
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

class Mexc::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "mexc");

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
        const int limit = 1000;
        int max_loops = 100;

        // Mexc intervals: 1m, 5m, 15m, 30m, 60m, 4h, 1d, 1M
        std::string interval = "1m";
        int64_t interval_ms = 60000;
        if (timeframe == "1m") { interval = "1m"; interval_ms = 60000; }
        else if (timeframe == "5m") { interval = "5m"; interval_ms = 300000; }
        else if (timeframe == "15m") { interval = "15m"; interval_ms = 900000; }
        else if (timeframe == "30m") { interval = "30m"; interval_ms = 1800000; }
        else if (timeframe == "1h") { interval = "60m"; interval_ms = 3600000; }
        else if (timeframe == "4h") { interval = "4h"; interval_ms = 14400000; }
        else if (timeframe == "1d") { interval = "1d"; interval_ms = 86400000; }
        else if (timeframe == "1M") { interval = "1M"; interval_ms = 2592000000; } // approx

        while (current_from < to_date) {
            int64_t chunk_end = current_from + (limit * interval_ms);
            // Strict Window Chunking: ensure we don't request too far ahead,
            // but also don't request past the ultimate to_date
            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "mexc", "", "");

            std::string query_string = "symbol=" + instrument_name + "&interval=" + interval;
            if (current_from > 0) query_string += "&startTime=" + std::to_string(current_from);
            // Note: MEXC returns [startTime, endTime].
            // If we don't set endTime, it might return up to current time or limit.
            // Let's set endTime to strictly control the window.
            if (chunk_end > 0) query_string += "&endTime=" + std::to_string(chunk_end);

            query_string += "&limit=" + std::to_string(limit);

            request.appendParam({
                {CCAPI_HTTP_PATH, "/api/v3/klines"},
                {CCAPI_HTTP_METHOD, "GET"},
                {CCAPI_HTTP_QUERY_STRING, query_string}
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
                                        std::string json_str = element.getValue(CCAPI_HTTP_BODY);
                                        rapidjson::Document doc;
                                        doc.Parse(json_str.c_str());

                                        if (!doc.HasParseError() && doc.IsArray()) {
                                            for (const auto& item : doc.GetArray()) {
                                                // [1660124280000, "24250", "24250.01", "24244.66", "24245.01", "3.08", "74697.518", "1660124339999"]
                                                if (item.IsArray() && item.Size() >= 6) {
                                                    Candle candle;
                                                    if (item[0].IsInt64()) candle.timestamp = item[0].GetInt64();
                                                    else if (item[0].IsString()) candle.timestamp = std::stoll(item[0].GetString());

                                                    auto getVal = [](const rapidjson::Value& v) -> double {
                                                        if (v.IsString()) return std::stod(v.GetString());
                                                        if (v.IsDouble()) return v.GetDouble();
                                                        return 0.0;
                                                    };

                                                    candle.open = getVal(item[1]);
                                                    candle.high = getVal(item[2]);
                                                    candle.low = getVal(item[3]);
                                                    candle.close = getVal(item[4]);
                                                    candle.volume = getVal(item[5]);

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                            success = true;
                                        }
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] MEXC Error: " << message.toString() << std::endl;
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

                // Advance current_from to the end of this batch or chunk
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

Mexc::Mexc(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Mexc::~Mexc() = default;

std::vector<Instrument> Mexc::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Mexc::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
