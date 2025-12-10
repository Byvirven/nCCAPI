#include "nccapi/exchanges/cryptocom.hpp"
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

class Cryptocom::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "cryptocom", "", "");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/v2/public/get-instruments"},
            {CCAPI_HTTP_METHOD, "GET"}
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

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("result") && doc["result"].HasMember("instruments")) {
                                        const auto& insts = doc["result"]["instruments"];
                                        for (const auto& item : insts.GetArray()) {
                                            Instrument instrument;
                                            instrument.id = item["instrument_name"].GetString();
                                            instrument.base = item["base_currency"].GetString();
                                            instrument.quote = item["quote_currency"].GetString();

                                            if (item.HasMember("price_decimals")) {
                                                int decimals = item["price_decimals"].GetInt();
                                                instrument.tick_size = std::pow(10.0, -decimals);
                                            }
                                            if (item.HasMember("quantity_decimals")) {
                                                int decimals = item["quantity_decimals"].GetInt();
                                                instrument.step_size = std::pow(10.0, -decimals);
                                            }

                                            if (!instrument.base.empty() && !instrument.quote.empty()) {
                                                instrument.symbol = instrument.base + "/" + instrument.quote;
                                            } else {
                                                instrument.symbol = instrument.id;
                                            }
                                            instrument.type = "spot";

                                            instruments.push_back(instrument);
                                        }
                                        return instruments;
                                    }
                                }
                            }
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
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "cryptocom", "", "");

        std::string period = "1m";
        if (timeframe == "1m") period = "1m";
        else if (timeframe == "5m") period = "5m";
        else if (timeframe == "15m") period = "15m";
        else if (timeframe == "30m") period = "30m";
        else if (timeframe == "1h") period = "1h";
        else if (timeframe == "4h") period = "4h";
        else if (timeframe == "6h") period = "6h";
        else if (timeframe == "12h") period = "12h";
        else if (timeframe == "1d") period = "1D";
        else if (timeframe == "1w") period = "7D";
        else if (timeframe == "2w") period = "14D";
        else if (timeframe == "1M") period = "1M";

        std::string query = "instrument_name=" + instrument_name + "&timeframe=" + period;

        request.appendParam({
            {CCAPI_HTTP_PATH, "/v2/public/get-candlestick"},
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

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("result") && doc["result"].HasMember("data")) {
                                        const auto& data = doc["result"]["data"];
                                        for (const auto& item : data.GetArray()) {
                                            if (item.IsObject()) {
                                                Candle candle;
                                                if (item.HasMember("t")) candle.timestamp = item["t"].GetInt64();

                                                auto getVal = [](const rapidjson::Value& v) -> double {
                                                    if(v.IsString()) return std::stod(v.GetString());
                                                    if(v.IsDouble()) return v.GetDouble();
                                                    if(v.IsInt64()) return (double)v.GetInt64();
                                                    return 0.0;
                                                };

                                                if (item.HasMember("o")) candle.open = getVal(item["o"]);
                                                if (item.HasMember("h")) candle.high = getVal(item["h"]);
                                                if (item.HasMember("l")) candle.low = getVal(item["l"]);
                                                if (item.HasMember("c")) candle.close = getVal(item["c"]);
                                                if (item.HasMember("v")) candle.volume = getVal(item["v"]);

                                                candles.push_back(candle);
                                            }
                                        }
                                    }
                                }
                            }

                            std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                return a.timestamp < b.timestamp;
                            });

                            if (from_date > 0 || to_date > 0) {
                                auto it = std::remove_if(candles.begin(), candles.end(), [from_date, to_date](const Candle& c) {
                                    if (from_date > 0 && c.timestamp < from_date) return true;
                                    if (to_date > 0 && c.timestamp >= to_date) return true;
                                    return false;
                                });
                                candles.erase(it, candles.end());
                            }

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

Cryptocom::Cryptocom(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Cryptocom::~Cryptocom() = default;

std::vector<Instrument> Cryptocom::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Cryptocom::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
