#include "nccapi/exchanges/kraken-futures.hpp"
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

class KrakenFutures::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "kraken-futures", "", "GET_INSTRUMENTS");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/derivatives/api/v3/instruments"},
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
                                    std::string json_str = element.getValue(CCAPI_HTTP_BODY);
                                    rapidjson::Document doc;
                                    doc.Parse(json_str.c_str());

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("instruments") && doc["instruments"].IsArray()) {
                                        for (const auto& s : doc["instruments"].GetArray()) {
                                            Instrument instrument;

                                            if (s.HasMember("symbol")) instrument.id = s["symbol"].GetString();
                                            else continue;

                                            if (s.HasMember("underlying")) instrument.base = s["underlying"].GetString();
                                            instrument.quote = "USD";

                                            if (s.HasMember("tickSize")) instrument.tick_size = s["tickSize"].GetDouble();
                                            if (s.HasMember("contractSize")) instrument.contract_multiplier = s["contractSize"].GetDouble();
                                            if (s.HasMember("contractValueTrade")) instrument.contract_size = s["contractValueTrade"].GetDouble();

                                            instrument.symbol = instrument.id;

                                            if (s.HasMember("tradeable")) {
                                                instrument.active = s["tradeable"].GetBool();
                                            }

                                            instrument.type = "future";
                                            if (s.HasMember("type")) instrument.type = s["type"].GetString();

                                            // Populate Info
                                            for (auto& m : s.GetObject()) {
                                                if (m.value.IsString()) {
                                                    instrument.info[m.name.GetString()] = m.value.GetString();
                                                } else if (m.value.IsNumber()) {
                                                    instrument.info[m.name.GetString()] = std::to_string(m.value.GetDouble());
                                                } else if (m.value.IsBool()) {
                                                    instrument.info[m.name.GetString()] = m.value.GetBool() ? "true" : "false";
                                                }
                                            }

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

        // Kraken Futures Generic Request
        // Endpoint: /api/charts/v1/trade/{symbol}/{resolution}
        // Resolution: 1m, 5m, 15m, 30m, 1h, 4h, 12h, 1d, 1w

        std::string resolution = "1m";
        if (timeframe == "1m") resolution = "1m";
        else if (timeframe == "5m") resolution = "5m";
        else if (timeframe == "15m") resolution = "15m";
        else if (timeframe == "30m") resolution = "30m";
        else if (timeframe == "1h") resolution = "1h";
        else if (timeframe == "4h") resolution = "4h";
        else if (timeframe == "12h") resolution = "12h";
        else if (timeframe == "1d") resolution = "1d";
        else if (timeframe == "1w") resolution = "1w";
        else resolution = "1m";

        std::string path = "/api/charts/v1/trade/" + instrument_name + "/" + resolution;

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "kraken-futures", "", "");
        request.appendParam({
            {"CCAPI_ENDPOINT", path},
            {"CCAPI_HTTP_METHOD", "GET"}
        });

        if (from_date > 0) {
             request.appendParam({{"from", std::to_string(from_date / 1000)}});
        }

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            std::vector<ccapi::Event> events = session->getEventQueue().purge();
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::GENERIC_PUBLIC_REQUEST) {
                            for (const auto& element : message.getElementList()) {
                                std::string json_content = element.getValue(CCAPI_HTTP_BODY);
                                rapidjson::Document doc;
                                doc.Parse(json_content.c_str());

                                if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("candles") && doc["candles"].IsArray()) {
                                    for (const auto& kline : doc["candles"].GetArray()) {
                                        if (kline.IsObject()) {
                                            Candle candle;
                                            if (kline.HasMember("time") && kline["time"].IsInt64()) candle.timestamp = kline["time"].GetInt64();
                                            if (kline.HasMember("open") && kline["open"].IsString()) candle.open = std::stod(kline["open"].GetString());
                                            if (kline.HasMember("high") && kline["high"].IsString()) candle.high = std::stod(kline["high"].GetString());
                                            if (kline.HasMember("low") && kline["low"].IsString()) candle.low = std::stod(kline["low"].GetString());
                                            if (kline.HasMember("close") && kline["close"].IsString()) candle.close = std::stod(kline["close"].GetString());
                                            if (kline.HasMember("volume") && kline["volume"].IsString()) candle.volume = std::stod(kline["volume"].GetString());
                                            else if (kline.HasMember("volume") && kline["volume"].IsNumber()) candle.volume = kline["volume"].GetDouble();

                                            candles.push_back(candle);
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

KrakenFutures::KrakenFutures(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
KrakenFutures::~KrakenFutures() = default;

std::vector<Instrument> KrakenFutures::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> KrakenFutures::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
