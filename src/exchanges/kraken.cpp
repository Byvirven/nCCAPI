#include "nccapi/exchanges/kraken.hpp"
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

class Kraken::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "kraken");

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

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

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

        std::string interval = "1";
        if (timeframe == "1m") interval = "1";
        else if (timeframe == "5m") interval = "5";
        else if (timeframe == "15m") interval = "15";
        else if (timeframe == "30m") interval = "30";
        else if (timeframe == "1h") interval = "60";
        else if (timeframe == "4h") interval = "240";
        else if (timeframe == "1d") interval = "1440";
        else if (timeframe == "1w") interval = "10080";
        else interval = "1";

        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "kraken", "", "");

        std::string query_string = "pair=" + instrument_name + "&interval=" + interval;
        if (from_date > 0) {
            query_string += "&since=" + std::to_string(from_date / 1000);
        }

        // Use CCAPI_HTTP_QUERY_STRING as advised
        request.appendParam({
            {CCAPI_HTTP_METHOD, "GET"},
            {CCAPI_HTTP_PATH, "/0/public/OHLC"},
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
                                if (element.has(CCAPI_HTTP_BODY)) {
                                    std::string json_content = element.getValue(CCAPI_HTTP_BODY);
                                    rapidjson::Document doc;
                                    doc.Parse(json_content.c_str());

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("result") && doc["result"].IsObject()) {
                                        const auto& result = doc["result"];
                                        // Result keys are pair names, e.g. "XXBTZUSD". We iterate members.
                                        for (auto itr = result.MemberBegin(); itr != result.MemberEnd(); ++itr) {
                                            if (std::string(itr->name.GetString()) != "last" && itr->value.IsArray()) {
                                                for (const auto& kline : itr->value.GetArray()) {
                                                    if (kline.IsArray() && kline.Size() >= 6) {
                                                        Candle candle;
                                                        if (kline[0].IsInt64()) candle.timestamp = kline[0].GetInt64() * 1000;
                                                        else if (kline[0].IsDouble()) candle.timestamp = (int64_t)(kline[0].GetDouble() * 1000);

                                                        if (kline[1].IsString()) candle.open = std::stod(kline[1].GetString());
                                                        else if (kline[1].IsDouble()) candle.open = kline[1].GetDouble();

                                                        if (kline[2].IsString()) candle.high = std::stod(kline[2].GetString());
                                                        else if (kline[2].IsDouble()) candle.high = kline[2].GetDouble();

                                                        if (kline[3].IsString()) candle.low = std::stod(kline[3].GetString());
                                                        else if (kline[3].IsDouble()) candle.low = kline[3].GetDouble();

                                                        if (kline[4].IsString()) candle.close = std::stod(kline[4].GetString());
                                                        else if (kline[4].IsDouble()) candle.close = kline[4].GetDouble();

                                                        if (kline[6].IsString()) candle.volume = std::stod(kline[6].GetString());
                                                        else if (kline[6].IsDouble()) candle.volume = kline[6].GetDouble();

                                                        candles.push_back(candle);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            if (!candles.empty()) {
                                 if (from_date > 0 || to_date > 0) {
                                     candles.erase(std::remove_if(candles.begin(), candles.end(), [from_date, to_date](const Candle& c) {
                                         if (to_date > 0 && c.timestamp > to_date) return true;
                                         if (from_date > 0 && c.timestamp < from_date) return true;
                                         return false;
                                     }), candles.end());
                                 }

                                std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                    return a.timestamp < b.timestamp;
                                });
                                return candles;
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

Kraken::Kraken(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Kraken::~Kraken() = default;

std::vector<Instrument> Kraken::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Kraken::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
