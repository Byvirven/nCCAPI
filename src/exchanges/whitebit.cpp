#include "nccapi/exchanges/whitebit.hpp"
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

class Whitebit::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "whitebit", "", "");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v4/public/markets"},
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
                                    std::string json_content = element.getValue(CCAPI_HTTP_BODY);
                                    rapidjson::Document doc;
                                    doc.Parse(json_content.c_str());

                                    if (!doc.HasParseError() && doc.IsArray()) {
                                        for (const auto& item : doc.GetArray()) {
                                            Instrument instrument;
                                            if (item.HasMember("name")) {
                                                instrument.id = item["name"].GetString();
                                                instrument.symbol = instrument.id; // Usually e.g. BTC_USDT

                                                // Try to split
                                                size_t u = instrument.id.find('_');
                                                if (u != std::string::npos) {
                                                    instrument.base = instrument.id.substr(0, u);
                                                    instrument.quote = instrument.id.substr(u+1);
                                                }
                                            }
                                            instrument.type = "spot"; // WhiteBIT generic markets are spot

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
        std::vector<Candle> all_candles;

        int64_t current_from = from_date;
        const int limit = 1000; // WhiteBIT limit 1500 but let's use 1000 to be safe
        int max_loops = 100;

        // WhiteBIT: /api/v1/public/kline
        // Params: market, interval, limit, start, end

        std::string interval = "1m";
        int64_t interval_ms = 60000;
        if (timeframe == "1m") { interval = "1m"; interval_ms = 60000; }
        else if (timeframe == "3m") { interval = "3m"; interval_ms = 180000; }
        else if (timeframe == "5m") { interval = "5m"; interval_ms = 300000; }
        else if (timeframe == "15m") { interval = "15m"; interval_ms = 900000; }
        else if (timeframe == "30m") { interval = "30m"; interval_ms = 1800000; }
        else if (timeframe == "1h") { interval = "1h"; interval_ms = 3600000; }
        else if (timeframe == "2h") { interval = "2h"; interval_ms = 7200000; }
        else if (timeframe == "4h") { interval = "4h"; interval_ms = 14400000; }
        else if (timeframe == "6h") { interval = "6h"; interval_ms = 21600000; }
        else if (timeframe == "8h") { interval = "8h"; interval_ms = 28800000; }
        else if (timeframe == "12h") { interval = "12h"; interval_ms = 43200000; }
        else if (timeframe == "1d") { interval = "1d"; interval_ms = 86400000; }
        else if (timeframe == "3d") { interval = "3d"; interval_ms = 259200000; }
        else if (timeframe == "1w") { interval = "1w"; interval_ms = 604800000; }
        else if (timeframe == "1M") { interval = "1M"; interval_ms = 2592000000; }

        while (current_from < to_date) {
            int64_t chunk_end = current_from + (limit * interval_ms);
            // Strict Window Chunking to force getting older data first
            // If window is too large, API might default to "latest" relative to 'end', ignoring 'start'.
            // So we must bound 'end' closely to 'start'.
            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "whitebit", "", "");

            std::string query = "market=" + instrument_name + "&interval=" + interval + "&limit=" + std::to_string(limit);
            if (current_from > 0) query += "&start=" + std::to_string(current_from / 1000);
            if (chunk_end > 0) query += "&end=" + std::to_string(chunk_end / 1000);

            request.appendParam({
                {CCAPI_HTTP_PATH, "/api/v1/public/kline"},
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

                                        const rapidjson::Value* rows = nullptr;
                                        if (!doc.HasParseError()) {
                                            if (doc.IsArray()) rows = &doc;
                                            else if (doc.IsObject() && doc.HasMember("result") && doc["result"].IsArray()) rows = &doc["result"];
                                        }

                                        if (rows) {
                                            for (const auto& item : rows->GetArray()) {
                                                if (item.IsArray() && item.Size() >= 6) {
                                                    Candle candle;
                                                    candle.timestamp = static_cast<uint64_t>(item[0].GetInt64()) * 1000;
                                                    candle.open = std::stod(item[1].GetString());
                                                    candle.close = std::stod(item[2].GetString());
                                                    candle.high = std::stod(item[3].GetString());
                                                    candle.low = std::stod(item[4].GetString());
                                                    candle.volume = std::stod(item[5].GetString());

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                            success = true;
                                        }
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "WhiteBIT Error: " << message.toString() << std::endl;
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

Whitebit::Whitebit(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Whitebit::~Whitebit() = default;

std::vector<Instrument> Whitebit::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Whitebit::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
