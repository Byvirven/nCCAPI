#include "nccapi/exchanges/coinbase.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_util_private.h"
#include "rapidjson/document.h"

namespace nccapi {

class Coinbase::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "coinbase");

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

                                if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                    instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "online");
                                }

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

        // Coinbase native CCAPI implementation does not support GET_RECENT_CANDLESTICKS.
        // We must use GENERIC_PUBLIC_REQUEST.

        // Map timeframe (1m, 1h) to seconds if needed
        int interval_seconds = 60;
        if (timeframe == "1m") interval_seconds = 60;
        else if (timeframe == "5m") interval_seconds = 300;
        else if (timeframe == "15m") interval_seconds = 900;
        else if (timeframe == "1h") interval_seconds = 3600;
        else if (timeframe == "6h") interval_seconds = 21600;
        else if (timeframe == "1d") interval_seconds = 86400;
        else interval_seconds = 60;

        // Convert timestamps to ISO 8601
        // Using ccapi::UtilTime::getISOTimestamp from ccapi_util_private.h
        auto from_tp = ccapi::UtilTime::makeTimePointFromMilliseconds(from_date);
        auto to_tp = ccapi::UtilTime::makeTimePointFromMilliseconds(to_date);

        std::string start_iso = ccapi::UtilTime::getISOTimestamp(from_tp);
        std::string end_iso = ccapi::UtilTime::getISOTimestamp(to_tp);

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "coinbase");
        request.appendParam({
            {"HTTP_METHOD", "GET"},
            {"HTTP_PATH", "/products/" + instrument_name + "/candles"},
            {"granularity", std::to_string(interval_seconds)},
            {"start", start_iso},
            {"end", end_iso}
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
                                if (element.has("HTTP_BODY")) {
                                    std::string json_body = element.getValue("HTTP_BODY");
                                    rapidjson::Document doc;
                                    doc.Parse(json_body.c_str());

                                    if (doc.IsArray()) {
                                        for (const auto& item : doc.GetArray()) {
                                            // Coinbase candle: [ time, low, high, open, close, volume ]
                                            if (item.IsArray() && item.Size() >= 6) {
                                                Candle candle;
                                                // Time is seconds
                                                candle.timestamp = (int64_t)item[0].GetInt64() * 1000;
                                                candle.low = item[1].GetDouble();
                                                candle.high = item[2].GetDouble();
                                                candle.open = item[3].GetDouble();
                                                candle.close = item[4].GetDouble();
                                                candle.volume = item[5].GetDouble();

                                                candles.push_back(candle);
                                            }
                                        }

                                        // Sort by timestamp asc
                                        std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                            return a.timestamp < b.timestamp;
                                        });

                                        return candles;
                                    }
                                }
                            }
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            // std::cerr << "Error: " << message.toString() << std::endl;
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

Coinbase::Coinbase(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Coinbase::~Coinbase() = default;

std::vector<Instrument> Coinbase::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Coinbase::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
