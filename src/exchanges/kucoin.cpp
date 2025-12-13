#include "nccapi/exchanges/kucoin.hpp"
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

class Kucoin::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "kucoin");

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

                                if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                    instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "true");
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
        const int limit = 1500;
        int max_loops = 100;

        // Kucoin timeframe: 1min, 3min, 5min, 15min, 30min, 1hour, 2hour, 4hour, 6hour, 8hour, 12hour, 1day, 1week
        std::string type = "1min";
        int64_t interval_ms = 60000;
        if (timeframe == "1m") { type = "1min"; interval_ms = 60000; }
        else if (timeframe == "5m") { type = "5min"; interval_ms = 300000; }
        else if (timeframe == "15m") { type = "15min"; interval_ms = 900000; }
        else if (timeframe == "30m") { type = "30min"; interval_ms = 1800000; }
        else if (timeframe == "1h") { type = "1hour"; interval_ms = 3600000; }
        else if (timeframe == "4h") { type = "4hour"; interval_ms = 14400000; }
        else if (timeframe == "1d") { type = "1day"; interval_ms = 86400000; }
        else if (timeframe == "1w") { type = "1week"; interval_ms = 604800000; }

        while (current_from < to_date) {
            int64_t chunk_end = current_from + ((int64_t)limit * interval_ms);
            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "kucoin", "", "");

            std::string query = "symbol=" + instrument_name + "&type=" + type;
            if (current_from > 0) query += "&startAt=" + std::to_string(current_from / 1000);
            if (chunk_end > 0) query += "&endAt=" + std::to_string(chunk_end / 1000);

            request.appendParam({
                {CCAPI_HTTP_PATH, "/api/v1/market/candles"},
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
                                                if (kline.IsArray() && kline.Size() >= 7) {
                                                    Candle candle;
                                                    // [ time, open, close, high, low, volume, turnover ]
                                                    if (kline[0].IsString()) candle.timestamp = std::stoll(kline[0].GetString()) * 1000;
                                                    if (kline[1].IsString()) candle.open = std::stod(kline[1].GetString());
                                                    if (kline[3].IsString()) candle.high = std::stod(kline[3].GetString());
                                                    if (kline[4].IsString()) candle.low = std::stod(kline[4].GetString());
                                                    if (kline[2].IsString()) candle.close = std::stod(kline[2].GetString());
                                                    if (kline[5].IsString()) candle.volume = std::stod(kline[5].GetString());

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] Kucoin Error: " << message.toString() << std::endl;
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

Kucoin::Kucoin(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Kucoin::~Kucoin() = default;

std::vector<Instrument> Kucoin::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Kucoin::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
