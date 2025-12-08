#include "nccapi/exchanges/coinbase.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"
#include "ccapi_cpp/ccapi_macro.h"
#include "rapidjson/document.h"

namespace nccapi {

namespace {
    std::string timestamp_to_iso8601(int64_t timestamp_ms) {
        std::time_t t = timestamp_ms / 1000;
        std::tm tm = *std::gmtime(&t); // gmtime is not thread-safe but ok for this context usually. Use gmtime_r if needed.
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    int64_t get_timeframe_ms(const std::string& timeframe) {
        if (timeframe == "1m") return 60000;
        if (timeframe == "5m") return 300000;
        if (timeframe == "15m") return 900000;
        if (timeframe == "1h") return 3600000;
        if (timeframe == "6h") return 21600000;
        if (timeframe == "1d") return 86400000;
        return 60000;
    }
}

class Coinbase::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "coinbase", "", "");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/products"},
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

                                    if (!doc.HasParseError() && doc.IsArray()) {
                                        for (const auto& item : doc.GetArray()) {
                                            Instrument instrument;
                                            instrument.id = item["id"].GetString();
                                            instrument.base = item["base_currency"].GetString();
                                            instrument.quote = item["quote_currency"].GetString();

                                            if (item.HasMember("quote_increment")) instrument.tick_size = std::stod(item["quote_increment"].GetString());
                                            if (item.HasMember("base_increment")) instrument.step_size = std::stod(item["base_increment"].GetString());

                                            if (!instrument.base.empty() && !instrument.quote.empty()) {
                                                instrument.symbol = instrument.base + "/" + instrument.quote;
                                            } else {
                                                instrument.symbol = instrument.id;
                                            }
                                            instrument.type = "spot";

                                            if (item.HasMember("status")) {
                                                instrument.active = (std::string(item["status"].GetString()) == "online");
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

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "coinbase", "", "");

        std::string granularity = "60";
        if (timeframe == "1m") granularity = "60";
        else if (timeframe == "5m") granularity = "300";
        else if (timeframe == "15m") granularity = "900";
        else if (timeframe == "1h") granularity = "3600";
        else if (timeframe == "6h") granularity = "21600";
        else if (timeframe == "1d") granularity = "86400";
        else granularity = "60";

        std::string path = "/products/" + instrument_name + "/candles";

        // Floor start time to align with granularity to ensure inclusion
        int64_t tf_ms = get_timeframe_ms(timeframe);
        int64_t adjusted_from = from_date;
        if (adjusted_from > 0) {
            adjusted_from = (adjusted_from / tf_ms) * tf_ms;
        }

        std::map<std::string, std::string> params = {
            {CCAPI_HTTP_PATH, path},
            {CCAPI_HTTP_METHOD, "GET"},
            {"granularity", granularity}
        };

        if (adjusted_from > 0) {
            params["start"] = timestamp_to_iso8601(adjusted_from);
        }
        if (to_date > 0) {
             params["end"] = timestamp_to_iso8601(to_date);
        }

        request.appendParam(params);

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
                                            if (item.IsArray() && item.Size() >= 6) {
                                                Candle candle;
                                                candle.timestamp = item[0].GetInt64() * 1000;
                                                candle.low = item[1].GetDouble();
                                                candle.high = item[2].GetDouble();
                                                candle.open = item[3].GetDouble();
                                                candle.close = item[4].GetDouble();
                                                candle.volume = item[5].GetDouble();

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
                // Filter again just in case API returned extra, but use adjusted_from logic
                // Actually, if we requested specific range, we should trust it, but sorting is needed.
                // Coinbase returns newest first.
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

Coinbase::Coinbase(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Coinbase::~Coinbase() = default;

std::vector<Instrument> Coinbase::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Coinbase::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
