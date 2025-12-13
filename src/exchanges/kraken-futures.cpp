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
        std::vector<Candle> all_candles;

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

        std::string path = "/derivatives/api/v4/charts/trade/" + instrument_name + "/" + resolution;

        int64_t current_from = from_date;
        const int limit = 2000;
        int max_loops = 100;

        while (true) {
            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "kraken-futures", "", "");

            // Note: URL Params must be strings.
            // Using query string or params?
            // CCAPI: PATH is separate from QUERY STRING.
            // Kraken Futures takes 'from' and 'to' as query params?
            // "https://futures.kraken.com/derivatives/api/v4/charts/trade/PI_XBTUSD/1m?from=...&to=..."

            // Let's construct query string manually to be safe or use appendParam map.
            // Using appendParam with map adds to query string for GET.

            std::string query = "";
            if (current_from > 0) {
                 query += "from=" + std::to_string(current_from / 1000);
            }
            // Do not send 'to' to force fetching next batch?
            // Or send 'to=to_date'? If range is large, does it limit?
            // If we send 'to', and it returns up to 'to' but limited count?
            // It returned 1940 candles. So it seems limited by count/time.
            // Let's send 'to' only if it's the target end.
            if (to_date > 0) {
                 if (query.length() > 0) query += "&";
                 query += "to=" + std::to_string(to_date / 1000);
            }

            request.appendParam({
                {CCAPI_HTTP_PATH, path},
                {CCAPI_HTTP_METHOD, "GET"},
                {CCAPI_HTTP_QUERY_STRING, query}
            });

            session->sendRequest(request);

            std::vector<Candle> batch_candles;
            bool success = false;

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

                                        if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("candles") && doc["candles"].IsArray()) {
                                            for (const auto& kline : doc["candles"].GetArray()) {
                                                if (kline.IsObject()) {
                                                    Candle candle;
                                                    if (kline.HasMember("time") && kline["time"].IsInt64()) candle.timestamp = kline["time"].GetInt64();

                                                    if (kline.HasMember("open") && kline["open"].IsString()) candle.open = std::stod(kline["open"].GetString());
                                                    else if (kline.HasMember("open") && kline["open"].IsDouble()) candle.open = kline["open"].GetDouble();

                                                    if (kline.HasMember("high") && kline["high"].IsString()) candle.high = std::stod(kline["high"].GetString());
                                                    else if (kline.HasMember("high") && kline["high"].IsDouble()) candle.high = kline["high"].GetDouble();

                                                    if (kline.HasMember("low") && kline["low"].IsString()) candle.low = std::stod(kline["low"].GetString());
                                                    else if (kline.HasMember("low") && kline["low"].IsDouble()) candle.low = kline["low"].GetDouble();

                                                    if (kline.HasMember("close") && kline["close"].IsString()) candle.close = std::stod(kline["close"].GetString());
                                                    else if (kline.HasMember("close") && kline["close"].IsDouble()) candle.close = kline["close"].GetDouble();

                                                    if (kline.HasMember("volume") && kline["volume"].IsString()) candle.volume = std::stod(kline["volume"].GetString());
                                                    else if (kline.HasMember("volume") && kline["volume"].IsNumber()) candle.volume = kline["volume"].GetDouble();

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                 // std::cout << "[DEBUG] Kraken Futures Error: " << message.toString() << std::endl;
                                 success = true;
                            }
                        }
                    }
                }
                if (success) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (batch_candles.empty()) {
                break;
            }

            std::sort(batch_candles.begin(), batch_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });

            all_candles.insert(all_candles.end(), batch_candles.begin(), batch_candles.end());

            int64_t last_ts = batch_candles.back().timestamp;
            current_from = last_ts + 1; // +1ms

            if (current_from >= to_date) {
                break;
            }

            // If we received fewer than expected (e.g. 1200 is default limit?), we might be done.
            if (batch_candles.size() < 10) { // arbitrary small number
                 // Maybe end of data
            }

            if (--max_loops <= 0) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!all_candles.empty()) {
             std::sort(all_candles.begin(), all_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });
            auto last = std::unique(all_candles.begin(), all_candles.end(), [](const Candle& a, const Candle& b){
                return a.timestamp == b.timestamp;
            });
            all_candles.erase(last, all_candles.end());

             if (from_date > 0 || to_date > 0) {
                 auto it = std::remove_if(all_candles.begin(), all_candles.end(), [from_date, to_date](const Candle& c) {
                     if (to_date > 0 && c.timestamp > to_date) return true;
                     if (from_date > 0 && c.timestamp < from_date) return true;
                     return false;
                 });
                 all_candles.erase(it, all_candles.end());
             }
        }

        return all_candles;
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
