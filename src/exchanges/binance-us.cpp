#include "nccapi/exchanges/binance-us.hpp"
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

class BinanceUs::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "binance-us", "", "");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v3/exchangeInfo"},
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
                                    std::string json_str = element.getValue(CCAPI_HTTP_BODY);
                                    rapidjson::Document doc;
                                    doc.Parse(json_str.c_str());

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("symbols")) {
                                        const auto& symbols = doc["symbols"];
                                        if (symbols.IsArray()) {
                                            for (const auto& s : symbols.GetArray()) {
                                                Instrument instrument;
                                                instrument.id = s["symbol"].GetString();
                                                instrument.base = s["baseAsset"].GetString();
                                                instrument.quote = s["quoteAsset"].GetString();

                                                if (s.HasMember("filters") && s["filters"].IsArray()) {
                                                    for (const auto& f : s["filters"].GetArray()) {
                                                        std::string filterType = f["filterType"].GetString();
                                                        if (filterType == "PRICE_FILTER") {
                                                            instrument.tick_size = std::stod(f["tickSize"].GetString());
                                                        } else if (filterType == "LOT_SIZE") {
                                                            instrument.step_size = std::stod(f["stepSize"].GetString());
                                                            instrument.min_size = std::stod(f["minQty"].GetString());
                                                        } else if (filterType == "NOTIONAL") {
                                                            instrument.min_notional = std::stod(f["minNotional"].GetString());
                                                        }
                                                    }
                                                }

                                                if (!instrument.base.empty() && !instrument.quote.empty()) {
                                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                                } else {
                                                    instrument.symbol = instrument.id;
                                                }
                                                instrument.type = "spot";

                                                if (s.HasMember("status")) {
                                                    instrument.active = (std::string(s["status"].GetString()) == "TRADING");
                                                }

                                                instruments.push_back(instrument);
                                            }
                                            return instruments;
                                        }
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
        const int limit = 1000;
        int max_loops = 50;

        while (true) {
            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "binance-us", "", "");

            std::string query_string = "symbol=" + instrument_name + "&interval=" + timeframe;
            query_string += "&limit=" + std::to_string(limit);
            if (current_from > 0) {
                 query_string += "&startTime=" + std::to_string(current_from);
            }
            if (to_date > 0) {
                 query_string += "&endTime=" + std::to_string(to_date);
            }

            request.appendParam({
                {CCAPI_HTTP_METHOD, "GET"},
                {CCAPI_HTTP_PATH, "/api/v3/klines"},
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
                                            for (const auto& arr : doc.GetArray()) {
                                                if (arr.IsArray() && arr.Size() >= 6) {
                                                     // [1499040000000,      // Open time
                                                     //  "0.01634790",       // Open
                                                     //  "0.80000000",       // High
                                                     //  "0.01575800",       // Low
                                                     //  "0.01577100",       // Close
                                                     //  "148976.11427815",  // Volume
                                                     //  1499644799999,      // Close time
                                                     Candle candle;
                                                     candle.timestamp = arr[0].GetInt64();
                                                     candle.open = std::stod(arr[1].GetString());
                                                     candle.high = std::stod(arr[2].GetString());
                                                     candle.low = std::stod(arr[3].GetString());
                                                     candle.close = std::stod(arr[4].GetString());
                                                     candle.volume = std::stod(arr[5].GetString());

                                                     batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                std::cout << "[DEBUG] BinanceUS Error: " << message.toString() << std::endl;
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

            // Append to all
            all_candles.insert(all_candles.end(), batch_candles.begin(), batch_candles.end());

            // Prepare for next iteration
            int64_t last_ts = batch_candles.back().timestamp;

            // Note: Binance uses open time.
            // We need to advance startTime > last_open_time.
            // But how much? +1ms is safe for Binance as it finds the next candle after that time.
            current_from = last_ts + 1;

            if (current_from >= to_date) {
                break;
            }

            // If we got fewer than limit, we are likely at the end
            if (batch_candles.size() < limit) {
                break;
            }

            // Safety
            if (--max_loops <= 0) break;

             std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Final sort and dedupe just in case
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

BinanceUs::BinanceUs(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
BinanceUs::~BinanceUs() = default;

std::vector<Instrument> BinanceUs::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> BinanceUs::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
