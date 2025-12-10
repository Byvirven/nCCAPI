#include "nccapi/exchanges/gateio-perpetual-futures.hpp"
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

class GateioPerpetualFutures::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        std::vector<std::string> settles = {"usdt", "btc", "usd"}; // Common settlement currencies

        for (const auto& settle : settles) {
            ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "gateio-perpetual-futures");
            request.appendParam({
                {CCAPI_SETTLE_ASSET, settle}
            });

            session->sendRequest(request);

            auto start = std::chrono::steady_clock::now();
            bool received = false;
            while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
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
                                    instrument.settle = settle;

                                    std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                    if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                    std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                    if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                    std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                    if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                    instrument.contract_size = 1.0; // GateIO perp usually 1 contract

                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }
                                    instrument.type = "swap"; // Perpetual

                                    for (const auto& pair : element.getNameValueMap()) {
                                        instrument.info[std::string(pair.first)] = pair.second;
                                    }

                                    instruments.push_back(instrument);
                                }
                                received = true;
                            }
                        }
                    }
                }
                if (received) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return instruments;
    }

    std::vector<Candle> get_historical_candles(const std::string& instrument_name,
                                               const std::string& timeframe,
                                               int64_t from_date,
                                               int64_t to_date) {
        std::vector<Candle> candles;

        // GateIO Perp Generic Request
        // Endpoint: /api/v4/futures/{settle}/candlesticks
        // We need to guess the settle from the instrument name.
        // Usually ends in _USDT or _USD or _BTC
        std::string settle = "usdt";
        if (instrument_name.find("_USD") != std::string::npos) {
            if (instrument_name.find("_USDT") != std::string::npos) settle = "usdt";
            else settle = "usd";
        } else if (instrument_name.find("_BTC") != std::string::npos) {
            settle = "btc";
        }

        std::string path = "/api/v4/futures/" + settle + "/candlesticks";

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "gateio-perpetual-futures", "", "");

        // GateIO interval: 10s, 1m, 5m, 15m, 30m, 1h, 4h, 8h, 1d, 7d
        std::string interval = "1m";
        if (timeframe == "1m") interval = "1m";
        else if (timeframe == "5m") interval = "5m";
        else if (timeframe == "1h") interval = "1h";
        else if (timeframe == "1d") interval = "1d";

        std::string query = "contract=" + instrument_name + "&interval=" + interval;
        if (from_date > 0) {
            query += "&from=" + std::to_string(from_date / 1000);
            if (to_date > 0) {
                query += "&to=" + std::to_string(to_date / 1000);
            }
        } else {
            query += "&limit=2000";
        }

        request.appendParam({
            {CCAPI_HTTP_PATH, path},
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

                                    if (!doc.HasParseError() && doc.IsArray()) {
                                        for (const auto& item : doc.GetArray()) {
                                            if (item.IsObject()) {
                                                Candle candle;
                                                if (item.HasMember("t")) candle.timestamp = (int64_t)item["t"].GetInt64() * 1000;
                                                if (item.HasMember("o")) candle.open = std::stod(item["o"].GetString());
                                                if (item.HasMember("h")) candle.high = std::stod(item["h"].GetString());
                                                if (item.HasMember("l")) candle.low = std::stod(item["l"].GetString());
                                                if (item.HasMember("c")) candle.close = std::stod(item["c"].GetString());
                                                if (item.HasMember("v")) candle.volume = (double)item["v"].GetInt64(); // Volume is integer number of contracts

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

GateioPerpetualFutures::GateioPerpetualFutures(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
GateioPerpetualFutures::~GateioPerpetualFutures() = default;

std::vector<Instrument> GateioPerpetualFutures::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> GateioPerpetualFutures::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
