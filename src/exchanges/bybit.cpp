#include "nccapi/exchanges/bybit.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

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
            ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bybit");
            request.appendParam({{"category", cat}});

            session->sendRequest(request);

            auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
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

                                    if (cat == "spot") instrument.type = "spot";
                                    else if (cat == "linear") instrument.type = "future";
                                    else if (cat == "inverse") instrument.type = "future";
                                    else if (cat == "option") instrument.type = "option";

                                    if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                        instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "Trading");
                                    }

                                    for (const auto& pair : element.getNameValueMap()) {
                                        instrument.info[std::string(pair.first)] = pair.second;
                                    }

                                    instruments.push_back(instrument);
                                }
                            }
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return instruments;
    }

    std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                               const std::string& timeframe,
                                               int64_t from_date,
                                               int64_t to_date) {
        std::vector<Candle> all_candles;

        // Priority categories to try. Linear (Perp) first, then Spot, then Inverse.
        std::vector<std::string> categories_to_try = {"linear", "spot", "inverse"};

        // If the user provided symbol suggests specific type, we might optimize, but for now try all.

        for (const auto& category : categories_to_try) {
            std::vector<Candle> cat_candles = fetch_candles_recursive(category, instrument_name, timeframe, from_date, to_date);
            if (!cat_candles.empty()) {
                // If we found data in this category, return it.
                // Note: This assumes the symbol ID is valid for only one category OR we prefer the first match.
                // Given standard usage, this is a reasonable heuristic.
                return cat_candles;
            }
        }

        return all_candles;
    }

    std::vector<Candle> fetch_candles_recursive(const std::string& category,
                                              const std::string& instrument_name,
                                              const std::string& timeframe,
                                              int64_t from_date,
                                              int64_t to_date) {
        std::vector<Candle> candles;
        int64_t current_from = from_date;
        int interval_sec = timeframeToSeconds(timeframe);
        int64_t interval_ms = interval_sec * 1000;
        const int limit = 1000;

        int max_loops = 100; // Safety

        while (current_from < to_date) {
            // Strict Window Chunking
            // Calculate chunk end based on limit
            int64_t chunk_end = current_from + (limit * interval_ms);
            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bybit", "", "");

            // Map timeframe to Bybit interval format
            std::string bybit_interval = timeframe;
            if (timeframe == "1m") bybit_interval = "1";
            else if (timeframe == "3m") bybit_interval = "3";
            else if (timeframe == "5m") bybit_interval = "5";
            else if (timeframe == "15m") bybit_interval = "15";
            else if (timeframe == "30m") bybit_interval = "30";
            else if (timeframe == "1h") bybit_interval = "60";
            else if (timeframe == "2h") bybit_interval = "120";
            else if (timeframe == "4h") bybit_interval = "240";
            else if (timeframe == "6h") bybit_interval = "360";
            else if (timeframe == "12h") bybit_interval = "720";
            else if (timeframe == "1d") bybit_interval = "D";
            else if (timeframe == "1w") bybit_interval = "W";
            else if (timeframe == "1M") bybit_interval = "M";

            std::string query_string = "category=" + category + "&symbol=" + instrument_name + "&interval=" + bybit_interval;
            query_string += "&start=" + std::to_string(current_from);
            query_string += "&end=" + std::to_string(chunk_end);
            query_string += "&limit=" + std::to_string(limit);

            request.appendParam({
                {CCAPI_HTTP_METHOD, "GET"},
                {CCAPI_HTTP_PATH, "/v5/market/kline"},
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

                                        // Response: {"retCode":0, "result": {"list": [ ["ts", "o", "h", "l", "c", "v", "turnover"], ... ]}}
                                        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("retCode") && doc["retCode"].GetInt() == 0) {
                                            if (doc.HasMember("result") && doc["result"].IsObject()) {
                                                const auto& result = doc["result"];
                                                if (result.HasMember("list") && result["list"].IsArray()) {
                                                    for (const auto& arr : result["list"].GetArray()) {
                                                        if (arr.IsArray() && arr.Size() >= 5) {
                                                            Candle candle;
                                                            candle.timestamp = std::stoll(arr[0].GetString());
                                                            candle.open = std::stod(arr[1].GetString());
                                                            candle.high = std::stod(arr[2].GetString());
                                                            candle.low = std::stod(arr[3].GetString());
                                                            candle.close = std::stod(arr[4].GetString());
                                                            candle.volume = std::stod(arr[5].GetString());

                                                            batch_candles.push_back(candle);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] Bybit Error: " << message.toString() << std::endl;
                                success = true;
                            }
                        }
                    }
                }
                if (success) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (batch_candles.empty()) {
                break;
            }

            // Bybit returns newest first (descending). We usually want ascending.
            // But here we insert to a list.
            // Let's sort this batch ascending first.
            std::sort(batch_candles.begin(), batch_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });

            candles.insert(candles.end(), batch_candles.begin(), batch_candles.end());

            // Prepare for next iteration
            // Since we used strict chunking [current_from, chunk_end], we move to chunk_end.
            current_from = chunk_end;

             if (--max_loops <= 0) break;
             std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Final Sort and Dedupe
        if (!candles.empty()) {
             std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });
            auto last = std::unique(candles.begin(), candles.end(), [](const Candle& a, const Candle& b){
                return a.timestamp == b.timestamp;
            });
            candles.erase(last, candles.end());
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
