#include "nccapi/exchanges/huobi-usdt-swap.hpp"
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

class HuobiUsdtSwap::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "huobi-usdt-swap");

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

                                std::string price_inc = element.getValue(CCAPI_ORDER_PRICE_INCREMENT);
                                if (!price_inc.empty()) { try { instrument.tick_size = std::stod(price_inc); } catch(...) {} }

                                // USDT Swap usually has contract_size
                                if(element.has(CCAPI_CONTRACT_SIZE)) {
                                     std::string val = element.getValue(CCAPI_CONTRACT_SIZE);
                                     if(!val.empty()) { try { instrument.contract_size = std::stod(val); } catch(...) {} }
                                }

                                if (!instrument.base.empty() && !instrument.quote.empty()) {
                                    instrument.symbol = instrument.base + "/" + instrument.quote;
                                } else {
                                    instrument.symbol = instrument.id; // Fallback
                                }

                                instrument.type = "swap_linear";

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

        // Huobi USDT Swap (Linear)
        // Endpoint: /linear-swap-ex/market/history/kline
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, "huobi-usdt-swap", "", "");
        request.appendParam({
            {CCAPI_HTTP_PATH, "/linear-swap-ex/market/history/kline"},
            {CCAPI_HTTP_METHOD, "GET"}
        });

        // Huobi period: 1min, 5min, 15min, 30min, 60min, 4hour, 1day, 1mon, 1week, 1year
        std::string period = "1min";
        if (timeframe == "1m") period = "1min";
        else if (timeframe == "5m") period = "5min";
        else if (timeframe == "15m") period = "15min";
        else if (timeframe == "30m") period = "30min";
        else if (timeframe == "1h") period = "60min";
        else if (timeframe == "4h") period = "4hour";
        else if (timeframe == "1d") period = "1day";
        else if (timeframe == "1w") period = "1week";
        else if (timeframe == "1M") period = "1mon";
        else period = "1min";

        request.appendParam({
            {"contract_code", instrument_name},
            {"period", period},
            {"size", "200"} // Default recent
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
                                std::string json_content = element.getValue(CCAPI_HTTP_BODY);
                                rapidjson::Document doc;
                                doc.Parse(json_content.c_str());

                                if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("data") && doc["data"].IsArray()) {
                                    for (const auto& kline : doc["data"].GetArray()) {
                                        if (kline.IsObject()) {
                                            Candle candle;
                                            if (kline.HasMember("id") && kline["id"].IsInt64()) candle.timestamp = kline["id"].GetInt64() * 1000;
                                            if (kline.HasMember("open") && kline["open"].IsNumber()) candle.open = kline["open"].GetDouble();
                                            if (kline.HasMember("high") && kline["high"].IsNumber()) candle.high = kline["high"].GetDouble();
                                            if (kline.HasMember("low") && kline["low"].IsNumber()) candle.low = kline["low"].GetDouble();
                                            if (kline.HasMember("close") && kline["close"].IsNumber()) candle.close = kline["close"].GetDouble();
                                            if (kline.HasMember("vol") && kline["vol"].IsNumber()) candle.volume = kline["vol"].GetDouble();

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

HuobiUsdtSwap::HuobiUsdtSwap(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
HuobiUsdtSwap::~HuobiUsdtSwap() = default;

std::vector<Instrument> HuobiUsdtSwap::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> HuobiUsdtSwap::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
