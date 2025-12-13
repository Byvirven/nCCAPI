#include "nccapi/exchanges/huobi-usdt-swap.hpp"
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

class HuobiUsdtSwap::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "huobi-usdt-swap");

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
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

                                // USDT Swap usually has contract_size
                                if(element.has(CCAPI_CONTRACT_SIZE)) {
                                     std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                     if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                }

                                if (!instrument.base.empty() && !instrument.quote.empty()) {
                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                } else {
                                    instrument.symbol = instrument.id; // Fallback
                                }

                                instrument.type = "swap";

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
        const int limit = 2000;
        int max_loops = 100;

        // Huobi USDT Swap (Linear)
        // Endpoint: /linear-swap-ex/market/history/kline

        // Huobi period: 1min, 5min, 15min, 30min, 60min, 4hour, 1day, 1mon, 1week, 1year
        std::string period = "1min";
        int64_t interval_ms = 60000;
        if (timeframe == "1m") { period = "1min"; interval_ms = 60000; }
        else if (timeframe == "5m") { period = "5min"; interval_ms = 300000; }
        else if (timeframe == "15m") { period = "15min"; interval_ms = 900000; }
        else if (timeframe == "30m") { period = "30min"; interval_ms = 1800000; }
        else if (timeframe == "1h") { period = "60min"; interval_ms = 3600000; }
        else if (timeframe == "4h") { period = "4hour"; interval_ms = 14400000; }
        else if (timeframe == "1d") { period = "1day"; interval_ms = 86400000; }
        else if (timeframe == "1w") { period = "1week"; interval_ms = 604800000; }
        else if (timeframe == "1M") { period = "1mon"; interval_ms = 2592000000; }

        std::string symbol = instrument_name;
        if (symbol.find("-USDT") == std::string::npos) {
            symbol += "-USDT";
        }

        while (current_from < to_date) {
            int64_t chunk_end = current_from + (limit * interval_ms);
            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "huobi-usdt-swap", "", "");

            std::string query = "contract_code=" + symbol + "&period=" + period;
            if (current_from > 0 && chunk_end > 0) {
                 query += "&from=" + std::to_string(current_from / 1000);
                 query += "&to=" + std::to_string(chunk_end / 1000);
            } else {
                 query += "&size=" + std::to_string(limit);
            }

            request.appendParam({
                {CCAPI_HTTP_PATH, "/linear-swap-ex/market/history/kline"},
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
                                                if (kline.IsObject()) {
                                                    Candle candle;
                                                    if (kline.HasMember("id") && kline["id"].IsInt64()) candle.timestamp = kline["id"].GetInt64() * 1000;
                                                    if (kline.HasMember("open") && kline["open"].IsNumber()) candle.open = kline["open"].GetDouble();
                                                    if (kline.HasMember("high") && kline["high"].IsNumber()) candle.high = kline["high"].GetDouble();
                                                    if (kline.HasMember("low") && kline["low"].IsNumber()) candle.low = kline["low"].GetDouble();
                                                    if (kline.HasMember("close") && kline["close"].IsNumber()) candle.close = kline["close"].GetDouble();
                                                    if (kline.HasMember("vol") && kline["vol"].IsNumber()) candle.volume = kline["vol"].GetDouble();

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] Huobi USDT Swap Error: " << message.toString() << std::endl;
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
        }

        return all_candles;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

HuobiUsdtSwap::HuobiUsdtSwap(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
HuobiUsdtSwap::~HuobiUsdtSwap() = default;

std::vector<Instrument> HuobiUsdtSwap::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> HuobiUsdtSwap::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
