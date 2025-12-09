#include "nccapi/exchanges/whitebit.hpp"
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

class Whitebit::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "whitebit", "", "");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v4/public/markets"},
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

                                    if (!doc.HasParseError() && doc.IsArray()) {
                                        for (const auto& item : doc.GetArray()) {
                                            Instrument instrument;
                                            if (item.HasMember("name")) {
                                                instrument.id = item["name"].GetString();
                                                instrument.symbol = instrument.id; // Usually e.g. BTC_USDT

                                                // Try to split
                                                size_t u = instrument.id.find('_');
                                                if (u != std::string::npos) {
                                                    instrument.base = instrument.id.substr(0, u);
                                                    instrument.quote = instrument.id.substr(u+1);
                                                }
                                            }
                                            instrument.type = "spot"; // WhiteBIT generic markets are spot

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
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "whitebit", "", "");

        // WhiteBIT: /api/v1/public/kline (Older) or /api/v4/public/kline
        // V4: GET /api/v4/public/kline
        // Params: market, interval, limit, start, end

        std::string interval = "1m";
        if (timeframe == "1m") interval = "1m";
        else if (timeframe == "3m") interval = "3m";
        else if (timeframe == "5m") interval = "5m";
        else if (timeframe == "15m") interval = "15m";
        else if (timeframe == "30m") interval = "30m";
        else if (timeframe == "1h") interval = "1h";
        else if (timeframe == "2h") interval = "2h";
        else if (timeframe == "4h") interval = "4h";
        else if (timeframe == "6h") interval = "6h";
        else if (timeframe == "8h") interval = "8h";
        else if (timeframe == "12h") interval = "12h";
        else if (timeframe == "1d") interval = "1d";
        else if (timeframe == "3d") interval = "3d";
        else if (timeframe == "1w") interval = "1w";
        else if (timeframe == "1M") interval = "1M";

        std::string query = "market=" + instrument_name + "&interval=" + interval + "&limit=1000";
        if (from_date > 0) query += "&start=" + std::to_string(from_date / 1000);
        if (to_date > 0) query += "&end=" + std::to_string(to_date / 1000);

        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v1/public/kline"},
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

                                    // WhiteBIT returns: { "result": [ [time, open, close, high, low, vol_base, vol_quote], ... ] }
                                    // Wait, V4 might be different. Let's check V1 structure or V4.
                                    // V4 Example: "result": [ [ 1594242960, "9246.06", "9246.06", "9246.06", "9246.06", "0", "0" ] ]
                                    // But sometimes it's just array of array without "result" wrapper?
                                    // The docs say: { "result": [...] } but some endpoints return [...]

                                    const rapidjson::Value* rows = nullptr;
                                    if (!doc.HasParseError()) {
                                        if (doc.IsArray()) rows = &doc;
                                        else if (doc.IsObject() && doc.HasMember("result") && doc["result"].IsArray()) rows = &doc["result"];
                                        else if (doc.IsObject() && doc.HasMember("code")) {
                                            // Error
                                            std::cout << "WhiteBIT Error: " << json_content << std::endl;
                                        }
                                    }

                                    if (rows) {
                                        for (const auto& item : rows->GetArray()) {
                                            if (item.IsArray() && item.Size() >= 6) {
                                                Candle candle;
                                                candle.timestamp = static_cast<uint64_t>(item[0].GetInt64()) * 1000;
                                                candle.open = std::stod(item[1].GetString());
                                                candle.close = std::stod(item[2].GetString()); // Open, Close
                                                candle.high = std::stod(item[3].GetString());
                                                candle.low = std::stod(item[4].GetString());
                                                candle.volume = std::stod(item[5].GetString()); // Base vol

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

Whitebit::Whitebit(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Whitebit::~Whitebit() = default;

std::vector<Instrument> Whitebit::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Whitebit::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
