#include "nccapi/exchanges/bitget-futures.hpp"
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

class BitgetFutures::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        // Restore Native GET_INSTRUMENTS which worked (644 pairs)
        std::vector<std::string> productTypes = {"USDT-FUTURES", "COIN-FUTURES", "USDC-FUTURES"};

        for (const auto& pType : productTypes) {
             ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "bitget-futures");
             request.appendParam({{"productType", pType}});

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

                                    if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                        instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "normal");
                                    }

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
        // Keep GENERIC for candles as Native returned 0.
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "bitget-futures", "", "");

        // Bitget Futures Granularity: 1m, 5m, 15m, 30m, 1H, 4H, 12H, 1D, 1W
        std::string granularity = "1m";
        if (timeframe == "1m") granularity = "1m";
        else if (timeframe == "5m") granularity = "5m";
        else if (timeframe == "15m") granularity = "15m";
        else if (timeframe == "30m") granularity = "30m";
        else if (timeframe == "1h") granularity = "1H";
        else if (timeframe == "4h") granularity = "4H";
        else if (timeframe == "12h") granularity = "12H";
        else if (timeframe == "1d") granularity = "1D";
        else if (timeframe == "1w") granularity = "1W";
        else granularity = "1m";

        request.appendParam({
            {CCAPI_HTTP_PATH, "/api/mix/v1/market/candles"},
            {CCAPI_HTTP_METHOD, "GET"},
            {"symbol", instrument_name},
            {"granularity", granularity},
            {"startTime", std::to_string(from_date)}, // ms
            {"endTime", std::to_string(to_date)} // ms
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
                                            if (item.IsArray() && item.Size() >= 6) {
                                                Candle candle;
                                                candle.timestamp = std::stoll(item[0].GetString());
                                                candle.open = std::stod(item[1].GetString());
                                                candle.high = std::stod(item[2].GetString());
                                                candle.low = std::stod(item[3].GetString());
                                                candle.close = std::stod(item[4].GetString());
                                                candle.volume = std::stod(item[5].GetString()); // Base volume

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

BitgetFutures::BitgetFutures(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
BitgetFutures::~BitgetFutures() = default;

std::vector<Instrument> BitgetFutures::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> BitgetFutures::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
