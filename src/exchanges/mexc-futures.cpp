#include "nccapi/exchanges/mexc-futures.hpp"
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

class MexcFutures::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "mexc-futures");

        session->sendRequest(request);

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
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

                                if (element.has(CCAPI_SETTLE_ASSET)) instrument.settle = element.getValue(CCAPI_SETTLE_ASSET);

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                if(element.has(CCAPI_CONTRACT_SIZE)) {
                                     std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                     if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                }

                                std::string qty_inc = element.getValue(CCAPI_ORDER_QUANTITY_INCREMENT);
                                if (!qty_inc.empty()) { try { instrument.step_size = std::stod(qty_inc); } catch(...) {} }

                                if (!instrument.base.empty() && !instrument.quote.empty()) {
                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                } else {
                                    instrument.symbol = instrument.id; // Fallback
                                }

                                instrument.type = "future";

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

        // Use GENERIC_PUBLIC_REQUEST for MEXC Futures
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "mexc-futures", "", "GET_KLINE");

        // Mexc Futures timeframe: Min1, Min5, Min15, Min30, Min60, Hour4, Hour8, Day1, Week1, Month1
        std::string interval = "Min1";
        if (timeframe == "1m") interval = "Min1";
        else if (timeframe == "5m") interval = "Min5";
        else if (timeframe == "15m") interval = "Min15";
        else if (timeframe == "30m") interval = "Min30";
        else if (timeframe == "1h") interval = "Min60";
        else if (timeframe == "4h") interval = "Hour4";
        else if (timeframe == "8h") interval = "Hour8";
        else if (timeframe == "1d") interval = "Day1";
        else if (timeframe == "1w") interval = "Week1";
        else if (timeframe == "1M") interval = "Month1";
        else interval = "Min1";

        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/v1/contract/kline/" + instrument_name}, // Path param symbol
            {CCAPI_HTTP_METHOD, "GET"},
            {"interval", interval},
            {"start", std::to_string(from_date / 1000)}, // Seconds
            {"end", std::to_string(to_date / 1000)}
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

                                    if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data")) {
                                        const auto& data = doc["data"];
                                        // {"time": [...], "open": [...], ...}
                                        if (data.HasMember("time") && data["time"].IsArray()) {
                                            const auto& times = data["time"];
                                            const auto& opens = data["open"];
                                            const auto& highs = data["high"];
                                            const auto& lows = data["low"];
                                            const auto& closes = data["close"];
                                            const auto& vols = data["vol"];

                                            for (rapidjson::SizeType i = 0; i < times.Size(); i++) {
                                                Candle candle;
                                                candle.timestamp = (int64_t)times[i].GetInt64() * 1000;
                                                candle.open = opens[i].GetDouble();
                                                candle.high = highs[i].GetDouble();
                                                candle.low = lows[i].GetDouble();
                                                candle.close = closes[i].GetDouble();
                                                candle.volume = vols[i].GetDouble();
                                                candles.push_back(candle);
                                            }
                                            std::sort(candles.begin(), candles.end(), [](const Candle& a, const Candle& b) {
                                                return a.timestamp < b.timestamp;
                                            });
                                            return candles;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return candles;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

MexcFutures::MexcFutures(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
MexcFutures::~MexcFutures() = default;

std::vector<Instrument> MexcFutures::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> MexcFutures::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
