#include "nccapi/exchanges/bitfinex.hpp"
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

                                // Bitfinex doesn't return tick/step size in standard endpoint usually?
                                // But CCAPI might normalize it if available
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

        // Use GENERIC_PUBLIC_REQUEST for Bitfinex
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitfinex", "", "GET_CANDLES");

        // Bitfinex timeframe: 1m, 5m, 15m, 30m, 1h, 3h, 6h, 12h, 1D, 7D, 14D, 1M
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
        else if (timeframe == "1M") tf = "1M";
        else tf = "1m";

        // Bitfinex API: /v2/candles/trade:{timeframe}:{symbol}/hist
        // Symbol format: usually tBTCUSD for spot. nCCAPI ID is usually tBTCUSD.
        // nCCAPI ID for Bitfinex is usually formatted correctly by get_instruments (e.g. tBTCUSD).

        std::string path = "/v2/candles/trade:" + tf + ":" + instrument_name + "/hist";

        request.appendParam({
            {CCAPI_HTTP_PATH, path},
            {CCAPI_HTTP_METHOD, "GET"},
            {"limit", "1000"},
            {"start", std::to_string(from_date)}, // ms
            {"end", std::to_string(to_date)},
            {"sort", "1"} // old to new
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
                                            // [ MTS, OPEN, CLOSE, HIGH, LOW, VOLUME ]
                                            if (item.IsArray() && item.Size() >= 6) {
                                                Candle candle;
                                                candle.timestamp = item[0].GetInt64();
                                                candle.open = item[1].GetDouble();
                                                candle.close = item[2].GetDouble();
                                                candle.high = item[3].GetDouble();
                                                candle.low = item[4].GetDouble();
                                                candle.volume = item[5].GetDouble();

                                                candles.push_back(candle);
                                            }
                                        }
                                        // Sort by timestamp
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
