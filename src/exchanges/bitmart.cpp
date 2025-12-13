#include "nccapi/exchanges/bitmart.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"

#include "rapidjson/document.h"

namespace nccapi {

class Bitmart::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bitmart");

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
        const int limit = 200;
        int max_loops = 50;

        // Bitmart API: step in minutes.
        int step = 60;
        if (timeframe == "1m") step = 1;
        else if (timeframe == "3m") step = 3;
        else if (timeframe == "5m") step = 5;
        else if (timeframe == "15m") step = 15;
        else if (timeframe == "30m") step = 30;
        else if (timeframe == "45m") step = 45;
        else if (timeframe == "1h") step = 60;
        else if (timeframe == "2h") step = 120;
        else if (timeframe == "3h") step = 180;
        else if (timeframe == "4h") step = 240;
        else if (timeframe == "1d") step = 1440;
        else if (timeframe == "1w") step = 10080;
        else if (timeframe == "1M") step = 43200;

        while (true) {
            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitmart", instrument_name, "");

            std::string query = "symbol=" + instrument_name + "&step=" + std::to_string(step);
            query += "&limit=" + std::to_string(limit);
            if (current_from > 0) query += "&after=" + std::to_string(current_from / 1000);
            // We do NOT use 'before' when paginating forward with 'after'.

            request.appendParam({
                {CCAPI_HTTP_PATH, "/spot/quotation/v3/klines"},
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
                                            const auto& data = doc["data"];
                                            for (const auto& kline : data.GetArray()) {
                                                if (kline.IsArray() && kline.Size() >= 7) {
                                                    Candle candle;
                                                    if (kline[0].IsInt64()) candle.timestamp = static_cast<uint64_t>(kline[0].GetInt64()) * 1000;
                                                    else if (kline[0].IsDouble()) candle.timestamp = static_cast<uint64_t>(kline[0].GetDouble()) * 1000;
                                                    else if (kline[0].IsString()) candle.timestamp = static_cast<uint64_t>(std::stoll(kline[0].GetString())) * 1000;
                                                    else continue;

                                                    candle.open = std::stod(kline[1].GetString());
                                                    candle.high = std::stod(kline[2].GetString());
                                                    candle.low = std::stod(kline[3].GetString());
                                                    candle.close = std::stod(kline[4].GetString());
                                                    candle.volume = std::stod(kline[5].GetString());
                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        } else {
                                            // std::cout << "[DEBUG] Bitmart Parse Error or No Data: " << json_content << std::endl;
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                std::cout << "[DEBUG] Bitmart Error: " << message.toString() << std::endl;
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

            // Bitmart returns newest first? "The returned data is sorted in descending order of time."
            // We need ascending for pagination if we use 'after'.
            // If we use 'after', it returns data after 'after'.
            // Docs: "after: Query range start time".
            // So if sorted descending, it returns from (after + limit) down to (after)?
            // Or does it return from (after) up to (after + limit)?
            // Usually 'after' implies getting older data? No, 'after' usually means newer data.
            // But if response is descending, we get [newest ... oldest].
            // If we sort ascending: [oldest ... newest].
            // Oldest should be close to 'after'.

            std::sort(batch_candles.begin(), batch_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });

            all_candles.insert(all_candles.end(), batch_candles.begin(), batch_candles.end());

            int64_t last_ts = batch_candles.back().timestamp;
            current_from = last_ts + 1000; // +1 sec (since Bitmart uses sec)

            if (current_from >= to_date) {
                break;
            }

            if (batch_candles.size() < limit) {
                break;
            }

            if (--max_loops <= 0) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Final Sort and Filter
        if (!all_candles.empty()) {
            std::sort(all_candles.begin(), all_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });
            auto last = std::unique(all_candles.begin(), all_candles.end(), [](const Candle& a, const Candle& b){
                return a.timestamp == b.timestamp;
            });
            all_candles.erase(last, all_candles.end());
        }

        return all_candles;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

Bitmart::Bitmart(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Bitmart::~Bitmart() = default;

std::vector<Instrument> Bitmart::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Bitmart::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
