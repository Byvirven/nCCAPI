#include "nccapi/exchanges/gateio.hpp"
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

class Gateio::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "gateio");

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

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "gateio", "", "");

        // GateIO timeframe: 10s, 1m, 5m, 15m, 30m, 1h, 4h, 8h, 1d, 7d
        std::string interval = "1m";
        if (timeframe == "1m") interval = "1m";
        else if (timeframe == "5m") interval = "5m";
        else if (timeframe == "15m") interval = "15m";
        else if (timeframe == "30m") interval = "30m";
        else if (timeframe == "1h") interval = "1h";
        else if (timeframe == "4h") interval = "4h";
        else if (timeframe == "8h") interval = "8h";
        else if (timeframe == "1d") interval = "1d";
        else if (timeframe == "1w") interval = "7d";
        else interval = "1m";

        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v4/spot/candlesticks"},
            {CCAPI_HTTP_METHOD, "GET"},
            {"currency_pair", instrument_name},
            {"interval", interval},
            {"from", std::to_string(from_date / 1000)}, // seconds
            {"to", std::to_string(to_date / 1000)}
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

                                    if (!doc.HasParseError() && doc.IsArray()) {
                                        for (const auto& item : doc.GetArray()) {
                                            // [ timestamp (sec), volume, close, high, low, open, ... ] (strings)
                                            if (item.IsArray() && item.Size() >= 6) {
                                                Candle candle;
                                                candle.timestamp = std::stoll(item[0].GetString()) * 1000;
                                                candle.volume = std::stod(item[1].GetString()); // volume
                                                candle.close = std::stod(item[2].GetString());
                                                candle.high = std::stod(item[3].GetString());
                                                candle.low = std::stod(item[4].GetString());
                                                candle.open = std::stod(item[5].GetString());

                                                candles.push_back(candle);
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            return candles;
                        }
                    }
                }
            }
            if (!candles.empty()) {
                if (from_date > 0 || to_date > 0) {
                    candles.erase(std::remove_if(candles.begin(), candles.end(), [from_date, to_date](const Candle& c) {
                        if (from_date > 0 && c.timestamp < from_date) return true;
                        if (to_date > 0 && c.timestamp > to_date) return true;
                        return false;
                    }), candles.end());
                }
                std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                    return a.timestamp < b.timestamp;
                });
                return candles;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return candles;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

Gateio::Gateio(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Gateio::~Gateio() = default;

std::vector<Instrument> Gateio::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Gateio::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
