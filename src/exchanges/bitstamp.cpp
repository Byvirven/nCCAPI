#include "nccapi/exchanges/bitstamp.hpp"
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

class Bitstamp::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bitstamp");

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

        // Bitstamp uses /api/v2/ohlc/{symbol}/
        // We must manually construct URL
        std::string path = "/api/v2/ohlc/" + instrument_name + "/";

        std::string step = "60";
        if (timeframe == "1m") step = "60";
        else if (timeframe == "1h") step = "3600";
        else if (timeframe == "1d") step = "86400";

        int64_t current_from = from_date;
        const int limit = 1000;
        int max_loops = 50;

        while (true) {
            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitstamp", "", "");

            std::string query = "step=" + step + "&limit=" + std::to_string(limit);
            if (current_from > 0) query += "&start=" + std::to_string(current_from / 1000); // Bitstamp uses seconds
            // Note: Bitstamp 'start' is inclusive.

            request.appendParam({
                {CCAPI_HTTP_PATH, path},
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
                                        std::string json_str = element.getValue(CCAPI_HTTP_BODY);
                                        rapidjson::Document doc;
                                        doc.Parse(json_str.c_str());

                                        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data") && doc["data"].IsObject() && doc["data"].HasMember("ohlc") && doc["data"]["ohlc"].IsArray()) {
                                            for (const auto& item : doc["data"]["ohlc"].GetArray()) {
                                                if (item.IsObject()) {
                                                    Candle candle;
                                                    if (item.HasMember("timestamp")) {
                                                        std::string ts = item["timestamp"].GetString();
                                                        candle.timestamp = std::stoll(ts) * 1000;
                                                    }
                                                    if (item.HasMember("open")) candle.open = std::stod(item["open"].GetString());
                                                    if (item.HasMember("high")) candle.high = std::stod(item["high"].GetString());
                                                    if (item.HasMember("low")) candle.low = std::stod(item["low"].GetString());
                                                    if (item.HasMember("close")) candle.close = std::stod(item["close"].GetString());
                                                    if (item.HasMember("volume")) candle.volume = std::stod(item["volume"].GetString());

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] Bitstamp Error: " << message.toString() << std::endl;
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

            int64_t last_ts = batch_candles.back().timestamp;
            // Next start = last + step (in seconds for param, but here we track ms)
            // Bitstamp step is in seconds.
            current_from = last_ts + (std::stoi(step) * 1000);

            if (current_from >= to_date) {
                break;
            }

            if (batch_candles.size() < limit) {
                break;
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
        }

        return all_candles;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

Bitstamp::Bitstamp(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Bitstamp::~Bitstamp() = default;

std::vector<Instrument> Bitstamp::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Bitstamp::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
