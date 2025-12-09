#include "nccapi/exchanges/deribit.hpp"
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

class Deribit::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "deribit");
        request.appendParam({
            {"currency", "BTC"}, // Deribit requires currency, defaulting to BTC
            {"kind", "future"}
        });

        session->sendRequest(request);

        // Fetch ETH as well
        ccapi::Request request2(ccapi::Request::Operation::GET_INSTRUMENTS, "deribit");
        request2.appendParam({
            {"currency", "ETH"},
            {"kind", "future"}
        });
        session->sendRequest(request2);

        auto start = std::chrono::steady_clock::now();
        int responses = 0;
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
                                instrument.type = "future";

                                for (const auto& pair : element.getNameValueMap()) {
                                    instrument.info[std::string(pair.first)] = pair.second;
                                }

                                instruments.push_back(instrument);
                            }
                            responses++;
                            if (responses >= 2) return instruments; // Got BTC and ETH
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                             // Ignore error, might just be one currency failing
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

        // Deribit Generic Request
        // Endpoint: /api/v2/public/get_tradingview_chart_data
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "deribit", "", "");

        // Deribit resolution: 1, 3, 5, 10, 15, 30, 60, 120, 180, 360, 720, 1D
        std::string resolution = "1";
        if (timeframe == "1m") resolution = "1";
        else if (timeframe == "5m") resolution = "5";
        else if (timeframe == "1h") resolution = "60";
        else if (timeframe == "1d") resolution = "1D";

        std::string query = "instrument_name=" + instrument_name + "&resolution=" + resolution;
        if (from_date > 0) {
            query += "&start_timestamp=" + std::to_string(from_date);
        }
        if (to_date > 0) {
            query += "&end_timestamp=" + std::to_string(to_date);
        }

        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v2/public/get_tradingview_chart_data"},
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
                                    std::string json_str = element.getValue(CCAPI_HTTP_BODY);
                                    rapidjson::Document doc;
                                    doc.Parse(json_str.c_str());

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("result") && doc["result"].IsObject()) {
                                        const auto& res = doc["result"];
                                        if (res.HasMember("ticks") && res["ticks"].IsArray()) {
                                            const auto& ticks = res["ticks"];
                                            const auto& opens = res["open"];
                                            const auto& highs = res["high"];
                                            const auto& lows = res["low"];
                                            const auto& closes = res["close"];
                                            const auto& volumes = res["volume"];

                                            for (size_t i = 0; i < ticks.Size(); ++i) {
                                                Candle candle;
                                                candle.timestamp = ticks[i].GetInt64();
                                                candle.open = opens[i].GetDouble();
                                                candle.high = highs[i].GetDouble();
                                                candle.low = lows[i].GetDouble();
                                                candle.close = closes[i].GetDouble();
                                                candle.volume = volumes[i].GetDouble();
                                                candles.push_back(candle);
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
