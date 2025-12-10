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
        std::vector<Candle> candles;
        // Pass instrument_name to Request constructor
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitmart", instrument_name, "");

        // Bitmart API: step in minutes.
        // 1, 3, 5, 15, 30, 45, 60, 120, 180, 240, 1440, 10080, 43200
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

        std::string query = "symbol=" + instrument_name + "&step=" + std::to_string(step);
        if (from_date > 0) query += "&after=" + std::to_string(from_date / 1000);
        if (to_date > 0) query += "&before=" + std::to_string(to_date / 1000);

        request.appendParam({
            {CCAPI_HTTP_PATH, "/spot/quotation/v3/klines"},
            {CCAPI_HTTP_METHOD, "GET"},
            {CCAPI_HTTP_QUERY_STRING, query}
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

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data") && doc["data"].IsArray()) {
                                        const auto& data = doc["data"];
                                        for (const auto& kline : data.GetArray()) {
                                            if (kline.IsArray() && kline.Size() >= 7) {
                                                Candle candle;
                                                // Bitmart returns [timestamp (sec), open, high, low, close, volume, ...]
                                                // Timestamp might be int or double
                                                if (kline[0].IsInt64()) candle.timestamp = static_cast<uint64_t>(kline[0].GetInt64()) * 1000;
                                                else if (kline[0].IsDouble()) candle.timestamp = static_cast<uint64_t>(kline[0].GetDouble()) * 1000;
                                                else continue;

                                                candle.open = std::stod(kline[1].GetString());
                                                candle.high = std::stod(kline[2].GetString());
                                                candle.low = std::stod(kline[3].GetString());
                                                candle.close = std::stod(kline[4].GetString());
                                                candle.volume = std::stod(kline[5].GetString());
                                                candles.push_back(candle);
                                            }
                                        }
                                    }
                                }
                            }

                            std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                return a.timestamp < b.timestamp;
                            });
                            return candles;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return candles;
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
