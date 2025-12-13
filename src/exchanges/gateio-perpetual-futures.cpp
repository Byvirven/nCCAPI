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

                                    instrument.contract_size = 1.0;

                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }
                                    instrument.type = "swap";

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
        std::vector<Candle> all_candles;

        std::string settle = "usdt";
        if (instrument_name.find("_USD") != std::string::npos) {
            if (instrument_name.find("_USDT") != std::string::npos) settle = "usdt";
            else settle = "usd";
        } else if (instrument_name.find("_BTC") != std::string::npos) {
            settle = "btc";
        }

        std::string path = "/api/v4/futures/" + settle + "/candlesticks";

        std::string interval = "1m";
        if (timeframe == "1m") interval = "1m";
        else if (timeframe == "5m") interval = "5m";
        else if (timeframe == "1h") interval = "1h";
        else if (timeframe == "1d") interval = "1d";

        int64_t current_to = to_date;
        const int limit = 2000;
        int max_loops = 100;

        while (true) {
            ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "gateio-perpetual-futures", "", "");

            std::string query = "contract=" + instrument_name + "&interval=" + interval;
            query += "&limit=" + std::to_string(limit);
            if (current_to > 0) query += "&to=" + std::to_string(current_to / 1000); // seconds
            // No 'from', rely on 'to' and 'limit' (backward)

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
                                                    if (item.HasMember("v")) candle.volume = (double)item["v"].GetInt64();

                                                    batch_candles.push_back(candle);
                                                }
                                            }
                                        }
                                        success = true;
                                    }
                                }
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
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

            int64_t oldest_ts = batch_candles.front().timestamp;
            current_to = oldest_ts - 1000; // -1 sec

            if (current_to < from_date) break;
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
                    if (from_date > 0 && c.timestamp < from_date) return true;
                    if (to_date > 0 && c.timestamp > to_date) return true;
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
