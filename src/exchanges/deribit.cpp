#include "nccapi/exchanges/deribit.hpp"
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

class Deribit::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "deribit");

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
                                // Deribit uses underlying symbol as base for options/futures often
                                instrument.base = element.getValue(CCAPI_BASE_ASSET);
                                instrument.quote = element.getValue(CCAPI_QUOTE_ASSET);

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                std::string min_qty = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                if (!min_qty.empty()) { try { instrument.min_size = std::stod(min_qty); } catch(...) {} }

                                if (element.has(CCAPI_UNDERLYING_SYMBOL)) {
                                    instrument.underlying = element.getValue(CCAPI_UNDERLYING_SYMBOL);
                                    if(instrument.base.empty()) instrument.base = instrument.underlying;
                                }
                                // Corrected field name
                                if (element.has(CCAPI_SETTLE_ASSET)) instrument.settle = element.getValue(CCAPI_SETTLE_ASSET);

                                if (element.has(CCAPI_CONTRACT_SIZE)) {
                                     std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                     if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                }

                                instrument.symbol = instrument.id; // Use ID as symbol for derivatives usually

                                // Determine type
                                if (instrument.id.find("-C") != std::string::npos || instrument.id.find("-P") != std::string::npos) {
                                    instrument.type = "option";
                                } else if (instrument.id.find("-PERPETUAL") != std::string::npos) {
                                    instrument.type = "swap";
                                } else {
                                    instrument.type = "future";
                                }

                                instrument.active = true; // Assume active if returned

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

        // Use GENERIC_PUBLIC_REQUEST for Deribit
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "deribit", "", "GET_CHART_DATA");

        // Deribit timeframe: 1, 3, 5, 10, 15, 30, 60, 120, 180, 360, 720, 1D
        std::string resolution = "1";
        if (timeframe == "1m") resolution = "1";
        else if (timeframe == "3m") resolution = "3";
        else if (timeframe == "5m") resolution = "5";
        else if (timeframe == "15m") resolution = "15";
        else if (timeframe == "30m") resolution = "30";
        else if (timeframe == "1h") resolution = "60";
        else if (timeframe == "2h") resolution = "120";
        else if (timeframe == "3h") resolution = "180";
        else if (timeframe == "6h") resolution = "360";
        else if (timeframe == "12h") resolution = "720";
        else if (timeframe == "1d") resolution = "1D";
        else resolution = "1";

        request.appendParam({
            {CCAPI_HTTP_PATH, "/public/get_tradingview_chart_data"},
            {CCAPI_HTTP_METHOD, "GET"},
            {"instrument_name", instrument_name},
            {"start_timestamp", std::to_string(from_date)}, // ms
            {"end_timestamp", std::to_string(to_date)},
            {"resolution", resolution}
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

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("result")) {
                                        const auto& result = doc["result"];
                                        // {"ticks": [t...], "open": [o...], ...}
                                        if (result.HasMember("ticks") && result["ticks"].IsArray()) {
                                            const auto& ticks = result["ticks"];
                                            const auto& opens = result["open"];
                                            const auto& highs = result["high"];
                                            const auto& lows = result["low"];
                                            const auto& closes = result["close"];
                                            const auto& vols = result["volume"];

                                            for (rapidjson::SizeType i = 0; i < ticks.Size(); i++) {
                                                Candle candle;
                                                candle.timestamp = ticks[i].GetInt64();
                                                candle.open = opens[i].GetDouble();
                                                candle.high = highs[i].GetDouble();
                                                candle.low = lows[i].GetDouble();
                                                candle.close = closes[i].GetDouble();
                                                candle.volume = vols[i].GetDouble();
                                                candles.push_back(candle);
                                            }
                                            std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                                return a.timestamp < b.timestamp;
                                            });
                                            return candles;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return candles;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

Deribit::Deribit(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Deribit::~Deribit() = default;

std::vector<Instrument> Deribit::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Deribit::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
