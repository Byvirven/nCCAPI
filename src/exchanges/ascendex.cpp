#include "nccapi/exchanges/ascendex.hpp"
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

class Ascendex::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "ascendex");

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

        std::string interval = "1";
        if (timeframe == "1m") interval = "1";
        else if (timeframe == "5m") interval = "5";
        else if (timeframe == "15m") interval = "15";
        else if (timeframe == "30m") interval = "30";
        else if (timeframe == "1h") interval = "60";
        else if (timeframe == "2h") interval = "120";
        else if (timeframe == "4h") interval = "240";
        else if (timeframe == "6h") interval = "360";
        else if (timeframe == "12h") interval = "720";
        else if (timeframe == "1d") interval = "1d";
        else if (timeframe == "1w") interval = "1w";
        else if (timeframe == "1M") interval = "1m";
        else interval = "1";

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "ascendex", "", "");

        std::string query_string = "symbol=" + instrument_name + "&interval=" + interval;
        if (from_date > 0) query_string += "&from=" + std::to_string(from_date);
        if (to_date > 0) query_string += "&to=" + std::to_string(to_date);

        request.appendParam({
            {CCAPI_HTTP_METHOD, "GET"},
            {CCAPI_HTTP_PATH, "/api/pro/v1/barhist"},
            {CCAPI_HTTP_QUERY_STRING, query_string}
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
                                // Debug: Print available keys
                                /*
                                std::cout << "[DEBUG] Available keys: ";
                                for(const auto& kv : element.getNameValueMap()) {
                                    std::cout << kv.first << " ";
                                }
                                std::cout << std::endl;
                                */

                                if (element.has(CCAPI_HTTP_BODY)) {
                                    std::string json_str = element.getValue(CCAPI_HTTP_BODY);
                                    // std::cout << "[DEBUG] Body: " << json_str << std::endl;
                                    rapidjson::Document doc;
                                    doc.Parse(json_str.c_str());

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data") && doc["data"].IsArray()) {
                                        for (const auto& bar : doc["data"].GetArray()) {
                                            if (bar.HasMember("data") && bar["data"].IsObject()) {
                                                const auto& c_obj = bar["data"];
                                                Candle candle;
                                                if (c_obj.HasMember("ts") && c_obj["ts"].IsInt64()) candle.timestamp = c_obj["ts"].GetInt64();

                                                if (c_obj.HasMember("o") && c_obj["o"].IsString()) candle.open = std::stod(c_obj["o"].GetString());
                                                if (c_obj.HasMember("h") && c_obj["h"].IsString()) candle.high = std::stod(c_obj["h"].GetString());
                                                if (c_obj.HasMember("l") && c_obj["l"].IsString()) candle.low = std::stod(c_obj["l"].GetString());
                                                if (c_obj.HasMember("c") && c_obj["c"].IsString()) candle.close = std::stod(c_obj["c"].GetString());
                                                if (c_obj.HasMember("v") && c_obj["v"].IsString()) candle.volume = std::stod(c_obj["v"].GetString());

                                                candles.push_back(candle);
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            std::cout << "[DEBUG] AscendEx Error: " << message.toString() << std::endl;
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

Ascendex::Ascendex(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Ascendex::~Ascendex() = default;

std::vector<Instrument> Ascendex::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Ascendex::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
