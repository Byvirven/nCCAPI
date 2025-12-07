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

        std::vector<std::string> settles = {"usdt", "btc", "usd"};

        for (const auto& settle : settles) {
            ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "gateio-perpetual-futures");
            request.appendParam({{"SETTLE_ASSET", settle}});

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

                                    std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                    if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                    std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                    if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                    std::string qty_min = element.getValue(CCAPI_ORDER_QUANTITY_MIN);
                                    if (!qty_min.empty()) { try { instrument.min_size = std::stod(qty_min); } catch(...) {} }

                                    if(element.has(CCAPI_CONTRACT_SIZE)) {
                                        std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                        if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                    }

                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }
                                    instrument.type = "future";
                                    instrument.settle = settle; // Store settle info

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
                if(received) break;
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

        // GateIO Perpetual Futures
        // Endpoint: /futures/{settle}/candlesticks
        // Determine settle from instrument_name?
        // E.g. BTC_USDT -> usdt. BTC_USD -> usd or btc?
        // Assuming USDT if ends with _USDT.
        std::string settle = "usdt";
        if (instrument_name.find("_USD") != std::string::npos && instrument_name.find("_USDT") == std::string::npos) settle = "usd";
        if (instrument_name.find("_BTC") != std::string::npos) settle = "btc"; // Just a guess, usually USD settle for inverse.
        // Actually, BTC_USD is usually inverse (settle BTC).
        // Let's rely on simple heuristic:
        if (instrument_name.rfind("_USDT") != std::string::npos) settle = "usdt";
        else if (instrument_name.rfind("_USD") != std::string::npos) settle = "usd";
        else if (instrument_name.rfind("_BTC") != std::string::npos) settle = "btc";

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "gateio-perpetual-futures", "", "");

        std::string interval = "1m";
        if (timeframe == "1m") interval = "1m";
        else if (timeframe == "5m") interval = "5m";
        else if (timeframe == "15m") interval = "15m";
        else if (timeframe == "30m") interval = "30m";
        else if (timeframe == "1h") interval = "1h";
        else if (timeframe == "4h") interval = "4h";
        else if (timeframe == "8h") interval = "8h";
        else if (timeframe == "1d") interval = "1d";
        else if (timeframe == "7d") interval = "7d";
        else interval = "1m";

        std::string path = "/api/v4/futures/" + settle + "/candlesticks";

        request.appendParam({
            {CCAPI_HTTP_PATH, path},
            {CCAPI_HTTP_METHOD, "GET"},
            {"contract", instrument_name},
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
                                            // [ t, v, c, h, l, o ]? No, check doc.
                                            // GateIO Futures: [ t, size, c, h, l, o ]?
                                            // Wait, docs say: [ t(seq), v(size), c, h, l, o ]
                                            // Same as spot usually.
                                            if (item.IsObject()) {
                                                // Wait, GateIO Spot returns array of arrays. Futures?
                                                // Docs: Array of objects for some, Array of Arrays for candlesticks.
                                                // Response: [ { "t": 123, ... } ] OR [ [123, ...] ]
                                                // /futures/{settle}/candlesticks returns list of objects!
                                                // { "t": 153..., "v": 100, "c": "...", "h": "...", "l": "...", "o": "..." }
                                                Candle candle;
                                                if (item.HasMember("t")) candle.timestamp = (int64_t)item["t"].GetDouble() * 1000;
                                                if (item.HasMember("v")) candle.volume = (double)item["v"].GetDouble(); // or GetInt64
                                                if (item.HasMember("c")) candle.close = std::stod(item["c"].GetString());
                                                if (item.HasMember("h")) candle.high = std::stod(item["h"].GetString());
                                                if (item.HasMember("l")) candle.low = std::stod(item["l"].GetString());
                                                if (item.HasMember("o")) candle.open = std::stod(item["o"].GetString());
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
