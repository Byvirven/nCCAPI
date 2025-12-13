#include "nccapi/exchanges/bitfinex.hpp"
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

class Bitfinex::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bitfinex");

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
        // Bitfinex API: /v2/candles/trade:1m:tBTCUSD/hist
        // Params: limit, start, end, sort

        std::string symbol = instrument_name;
        if (symbol.size() > 0 && symbol[0] != 't') symbol = "t" + symbol; // Ensure 't' prefix for trading pairs

        std::string tf = "1m";
        if (timeframe == "1m") tf = "1m";
        else if (timeframe == "5m") tf = "5m";
        else if (timeframe == "15m") tf = "15m";
        else if (timeframe == "30m") tf = "30m";
        else if (timeframe == "1h") tf = "1h";
        else if (timeframe == "3h") tf = "3h";
        else if (timeframe == "6h") tf = "6h";
        else if (timeframe == "12h") tf = "12h";
        else if (timeframe == "1d") tf = "1D";
        else if (timeframe == "1w") tf = "7D";
        else if (timeframe == "2w") tf = "14D";
        else if (timeframe == "1M") tf = "1M";

        std::string path = "/v2/candles/trade:" + tf + ":" + symbol + "/hist";

        int64_t current_from = from_date;
        const int limit = 10000; // Bitfinex max limit
        int max_loops = 50;

        while (true) {
            std::string query = "limit=" + std::to_string(limit) + "&sort=1"; // sort=1 for oldest first
            if (current_from > 0) query += "&start=" + std::to_string(current_from);
            if (to_date > 0) query += "&end=" + std::to_string(to_date);

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitfinex", "", "");
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
                                        std::string json_content = element.getValue(CCAPI_HTTP_BODY);
                                        rapidjson::Document doc;
                                        doc.Parse(json_content.c_str());

                                        if (!doc.HasParseError() && doc.IsArray()) {
                                            for (const auto& item : doc.GetArray()) {
                                                if (item.IsArray() && item.Size() >= 6) {
                                                    Candle candle;
                                                    // [ MTS, OPEN, CLOSE, HIGH, LOW, VOLUME ]
                                                    candle.timestamp = item[0].GetInt64();
                                                    candle.open = item[1].GetDouble();
                                                    candle.close = item[2].GetDouble();
                                                    candle.high = item[3].GetDouble();
                                                    candle.low = item[4].GetDouble();
                                                    candle.volume = item[5].GetDouble();

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] Bitfinex Error: " << message.toString() << std::endl;
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

            all_candles.insert(all_candles.end(), batch_candles.begin(), batch_candles.end());

            int64_t last_ts = batch_candles.back().timestamp;
            current_from = last_ts + 1; // Or interval? +1ms is safe for start param.

            if (current_from >= to_date) {
                break;
            }

            // If we got fewer than limit, we are done
            if (batch_candles.size() < limit) {
                break;
            }

            if (--max_loops <= 0) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Bitfinex 30req/min = 0.5req/sec = 2000ms delay?
            // 30 req/min is strict. 1 req every 2 seconds.
            // I should increase sleep.
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
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

            if (from_date > 0 || to_date > 0) {
                auto it = std::remove_if(all_candles.begin(), all_candles.end(), [from_date, to_date](const Candle& c) {
                    if (from_date > 0 && c.timestamp < from_date) return true;
                    if (to_date > 0 && c.timestamp > to_date) return true; // changed to > to_date (exclusive if desired? usually inclusive)
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

Bitfinex::Bitfinex(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Bitfinex::~Bitfinex() = default;

std::vector<Instrument> Bitfinex::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Bitfinex::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
