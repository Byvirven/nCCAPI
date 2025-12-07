#include "nccapi/exchanges/kraken.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "rapidjson/document.h"

namespace nccapi {

class Kraken::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "kraken");

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

                                if (element.has("WSNAME")) {
                                    // Kraken WSNAME is usually normalized "BASE/QUOTE"
                                    instrument.symbol = element.getValue("WSNAME");
                                } else if (!instrument.base.empty() && !instrument.quote.empty()) {
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

        // Use GENERIC_PUBLIC_REQUEST for Kraken
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "kraken", "", "GET_OHLC");

        // Kraken interval is in minutes
        int interval_minutes = 1;
        if (timeframe == "1m") interval_minutes = 1;
        else if (timeframe == "5m") interval_minutes = 5;
        else if (timeframe == "15m") interval_minutes = 15;
        else if (timeframe == "30m") interval_minutes = 30;
        else if (timeframe == "1h") interval_minutes = 60;
        else if (timeframe == "4h") interval_minutes = 240;
        else if (timeframe == "1d") interval_minutes = 1440;
        else if (timeframe == "1w") interval_minutes = 10080;
        else interval_minutes = 1;

        request.appendParam({
            {CCAPI_HTTP_PATH, "/0/public/OHLC"},
            {CCAPI_HTTP_METHOD, "GET"},
            {"pair", instrument_name},
            {"interval", std::to_string(interval_minutes)},
            {"since", std::to_string(from_date / 1000)} // Seconds
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

                                    if (!doc.HasParseError() && doc.HasMember("result")) {
                                        const auto& result = doc["result"];
                                        // Kraken returns a map where key is the pair name (which might differ from requested symbol)
                                        // We just iterate the first member that is an array
                                        for (auto& m : result.GetObject()) {
                                            if (m.value.IsArray()) {
                                                for (const auto& item : m.value.GetArray()) {
                                                    // [int <time>, string <open>, string <high>, string <low>, string <close>, string <vwap>, string <volume>, int <count>]
                                                    if (item.Size() >= 8) {
                                                        Candle candle;
                                                        candle.timestamp = (int64_t)item[0].GetInt64() * 1000;

                                                        // Kraken returns strings or numbers depending on API version/wrapper?
                                                        // Docs say strings/float. RapidJSON might see them as strings.
                                                        // Let's handle both.

                                                        auto getVal = [](const rapidjson::Value& v) -> double {
                                                            if (v.IsString()) return std::stod(v.GetString());
                                                            if (v.IsDouble()) return v.GetDouble();
                                                            if (v.IsInt64()) return (double)v.GetInt64();
                                                            return 0.0;
                                                        };

                                                        candle.open = getVal(item[1]);
                                                        candle.high = getVal(item[2]);
                                                        candle.low = getVal(item[3]);
                                                        candle.close = getVal(item[4]);
                                                        candle.volume = getVal(item[6]); // Volume is index 6

                                                        candles.push_back(candle);
                                                    }
                                                }
                                                // Break after finding the array (there's usually "last" field too)
                                                break;
                                            }
                                        }
                                        std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                            return a.timestamp < b.timestamp;
                                        });
                                        return candles;
                                    }
                                }
                            }
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
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

Kraken::Kraken(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Kraken::~Kraken() = default;

std::vector<Instrument> Kraken::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Kraken::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
