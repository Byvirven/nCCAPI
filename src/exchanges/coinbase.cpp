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

    std::string url_encode(const std::string &value) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
            std::string::value_type c = (*i);
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
                continue;
            }
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char) c);
            escaped << std::nouppercase;
        }
        return escaped.str();
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
        std::vector<Candle> all_candles;

        std::string granularity = "60";
        if (timeframe == "1m") granularity = "60";
        else if (timeframe == "5m") granularity = "300";
        else if (timeframe == "15m") granularity = "900";
        else if (timeframe == "1h") granularity = "3600";
        else if (timeframe == "6h") granularity = "21600";
        else if (timeframe == "1d") granularity = "86400";
        else granularity = "60";

        std::string path = "/products/" + instrument_name + "/candles";

        int64_t tf_ms = get_timeframe_ms(timeframe);
        int64_t current_from = from_date;
        if (current_from > 0) {
            current_from = (current_from / tf_ms) * tf_ms;
        }
        const int limit = 300;
        int max_loops = 50;

        while (current_from < to_date) {
            int64_t chunk_end = current_from + (limit * tf_ms);
            // Coinbase calculates number of candles = (end - start) / granularity.
            // If we want max 300, range should be 300 * granularity.
            // BUT: "If the start/end time are not aligned, they will be aligned to the start of the granularity bucket."
            // AND: "The maximum number of data points for a single request is 300."

            if (chunk_end > to_date) chunk_end = to_date;

            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "coinbase", "", "");

            std::string query_string = "granularity=" + granularity;
            query_string += "&start=" + url_encode(timestamp_to_iso8601(current_from));
            query_string += "&end=" + url_encode(timestamp_to_iso8601(chunk_end));

            std::map<std::string, std::string> params = {
                {CCAPI_HTTP_PATH, path},
                {CCAPI_HTTP_METHOD, "GET"},
                {CCAPI_HTTP_QUERY_STRING, query_string}
            };

            request.appendParam(params);

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

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "[DEBUG] Coinbase Error: " << message.toString() << std::endl;
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

            current_from = chunk_end;

            if (--max_loops <= 0) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(250)); // Coinbase rate limit safe
        }

        if (!all_candles.empty()) {
            std::sort(all_candles.begin(), all_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });
            auto last = std::unique(all_candles.begin(), all_candles.end(), [](const Candle& a, const Candle& b){
                return a.timestamp == b.timestamp;
            });
            all_candles.erase(last, all_candles.end());
        }

        return all_candles;
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
