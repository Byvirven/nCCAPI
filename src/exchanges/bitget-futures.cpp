#include "nccapi/exchanges/bitget-futures.hpp"
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

class BitgetFutures::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        std::vector<std::string> productTypes = {"USDT-FUTURES", "COIN-FUTURES", "USDC-FUTURES"};

        for (const auto& pType : productTypes) {
             ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bitget-futures");
             request.appendParam({{"productType", pType}});

             session->sendRequest(request);

            auto start = std::chrono::steady_clock::now();
            bool received = false;
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

                                    if(element.has(CCAPI_CONTRACT_SIZE)) {
                                        std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                        if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                    }

                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }
                                    instrument.type = "future";

                                    if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                        instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "normal");
                                    }

                                    for (const auto& pair : element.getNameValueMap()) {
                                        instrument.info[std::string(pair.first)] = pair.second;
                                    }

                                    instruments.push_back(instrument);
                                }
                                received = true;
                            }
                        }
                    }
                }
                if(received) break;
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

        int64_t current_from = from_date;
        int interval_sec = timeframeToSeconds(timeframe);
        int64_t interval_ms = interval_sec * 1000;
        const int limit = 1000; // Bitget Futures limit
        int max_loops = 100;

        while (current_from < to_date) {
            int64_t chunk_end = current_from + (limit * interval_ms);
            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitget-futures", "", "");

            std::string granularity = "1m";
            if (timeframe == "1m") granularity = "1m";
            else if (timeframe == "5m") granularity = "5m";
            else if (timeframe == "15m") granularity = "15m";
            else if (timeframe == "30m") granularity = "30m";
            else if (timeframe == "1h") granularity = "1H";
            else if (timeframe == "4h") granularity = "4H";
            else if (timeframe == "12h") granularity = "12H";
            else if (timeframe == "1d") granularity = "1D";
            else if (timeframe == "1w") granularity = "1W";
            else granularity = "1m";

            std::string symbol = instrument_name;
            size_t underscore = symbol.find("_");
            if (underscore != std::string::npos) {
                symbol = symbol.substr(0, underscore);
            }

            std::string productType = "USDT-FUTURES";
            if (instrument_name.find("USDT") != std::string::npos) {
                productType = "USDT-FUTURES";
            } else if (instrument_name.find("USDC") != std::string::npos) {
                productType = "USDC-FUTURES";
            } else if (instrument_name.find("USD") != std::string::npos && instrument_name.find("USDT") == std::string::npos) {
                productType = "COIN-FUTURES";
            }

            std::string query_string = "symbol=" + symbol + "&granularity=" + granularity + "&productType=" + productType;
            query_string += "&startTime=" + std::to_string(current_from);
            query_string += "&endTime=" + std::to_string(chunk_end);
            query_string += "&limit=" + std::to_string(limit);

            request.appendParam({
                {CCAPI_HTTP_PATH, "/api/v2/mix/market/candles"},
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

                                        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data") && doc["data"].IsArray()) {
                                            for (const auto& item : doc["data"].GetArray()) {
                                                if (item.IsArray() && item.Size() >= 6) {
                                                    Candle candle;
                                                    candle.timestamp = std::stoll(item[0].GetString());
                                                    candle.open = std::stod(item[1].GetString());
                                                    candle.high = std::stod(item[2].GetString());
                                                    candle.low = std::stod(item[3].GetString());
                                                    candle.close = std::stod(item[4].GetString());
                                                    candle.volume = std::stod(item[5].GetString());

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] Bitget Futures Error: " << message.toString() << std::endl;
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

            std::sort(batch_candles.begin(), batch_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });

            all_candles.insert(all_candles.end(), batch_candles.begin(), batch_candles.end());

            current_from = chunk_end;

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
        }

        return all_candles;
    }

    int timeframeToSeconds(const std::string& timeframe) {
        // Bitget Futures timeframes mapping to seconds
        if (timeframe == "1m") return 60;
        if (timeframe == "5m") return 300;
        if (timeframe == "15m") return 900;
        if (timeframe == "30m") return 1800;
        if (timeframe == "1h") return 3600;
        if (timeframe == "4h") return 14400;
        if (timeframe == "12h") return 43200;
        if (timeframe == "1d") return 86400;
        if (timeframe == "1w") return 604800;
        return 60;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

BitgetFutures::BitgetFutures(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
BitgetFutures::~BitgetFutures() = default;

std::vector<Instrument> BitgetFutures::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> BitgetFutures::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
