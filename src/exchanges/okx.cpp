#include "nccapi/exchanges/okx.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

#include "nccapi/sessions/unified_session.hpp"
#include "ccapi_cpp/ccapi_request.h"
#include "ccapi_cpp/ccapi_event.h"
#include "ccapi_cpp/ccapi_message.h"

namespace nccapi {

class Okx::Impl {
public:
    Impl(std::shared_ptr<UnifiedSession> s) : session(s) {}

    std::vector<Instrument> get_instruments() {
        std::vector<Instrument> instruments;
        // OKX requires instType: SPOT, SWAP, FUTURES, OPTION
        std::vector<std::string> instTypes = {"SPOT", "SWAP", "FUTURES", "OPTION"};

        for (const auto& instType : instTypes) {
            ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, "okx");
            request.appendParam({{"instType", instType}});

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

                                    // Symbol construction logic
                                    if (!instrument.base.empty() && !instrument.quote.empty()) {
                                        instrument.symbol = instrument.base + "/" + instrument.quote;
                                    } else {
                                        instrument.symbol = instrument.id;
                                    }

                                    // Map OKX type
                                    if (instType == "SPOT") instrument.type = "spot";
                                    else if (instType == "SWAP") instrument.type = "swap"; // Perpetual Swap
                                    else if (instType == "FUTURES") instrument.type = "future"; // Expiry Future
                                    else if (instType == "OPTION") instrument.type = "option";

                                    if (element.has(CCAPI_INSTRUMENT_STATUS)) {
                                        instrument.active = (element.getValue(CCAPI_INSTRUMENT_STATUS) == "live");
                                    }

                                    for (const auto& pair : element.getNameValueMap()) {
                                        instrument.info[std::string(pair.first)] = pair.second;
                                    }

                                    instruments.push_back(instrument);
                                }
                                received = true;
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // Log error or continue
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

        // OKX Pagination Strategy: Backward Pagination
        // The API supports `after` parameter: "Pagination of data to return records earlier than the requested ts."
        // We start requesting from `to_date` and move backwards.

        int64_t current_to = to_date;
        const int limit = 100; // Native CCAPI might use 100. Docs say 100 limit for market candles.
        int max_loops = 100;

        while (current_to > from_date) {
            ccapi::Request request(ccapi::Request::Operation::GET_HISTORICAL_CANDLESTICKS, "okx", instrument_name);

            // "after" = Pagination of data to return records EARLIER than the requested ts.
            // So we set `after` to `current_to`.
            // Wait, CCAPI maps parameters.
            // If we use CCAPI_END_TIME_SECONDS, CCAPI might map it to `after`?
            // Actually, OKX API uses `after` as an ID or timestamp for pagination.
            // Let's use native CCAPI mapping first, or generic if needed.
            // The existing implementation used CCAPI_START_TIME_SECONDS and CCAPI_END_TIME_SECONDS.
            // OKX API: GET /api/v5/market/candles?instId=BTC-USDT&bar=1m&after=...&limit=100
            // `after`: request data older than this timestamp.
            // `before`: request data newer than this timestamp.

            // If we use standard CCAPI params:
            // CCAPI_END_TIME_SECONDS -> probably maps to something, but let's check.
            // If I set CCAPI_END_TIME_SECONDS, CCAPI likely sets "after" if it's smart, or just passes params.

            // To be safe and precise with pagination, let's use GENERIC request or carefully set params.
            // But wait, the previous code was using native `GET_HISTORICAL_CANDLESTICKS`.
            // Let's stick to native but manage the loop.
            // CCAPI for OKX: `include/ccapi_cpp/service/ccapi_market_data_service_okx.h` uses `after` for `endTime`.

            // So:
            // Request 1: end = to_date. Returns [to_date - 100*interval, to_date]
            // We get the OLDEST candle's timestamp from the response.
            // Next Request: end = oldest_timestamp.

            request.appendParam({
                {CCAPI_CANDLESTICK_INTERVAL_SECONDS, std::to_string(timeframeToSeconds(timeframe))},
                //{CCAPI_START_TIME_SECONDS, std::to_string(from_date / 1000)}, // Don't restrict start for backward loop unless we check it manually
                {CCAPI_END_TIME_SECONDS, std::to_string(current_to / 1000)},
                {CCAPI_LIMIT, "100"}
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
                            if (message.getType() == ccapi::Message::Type::GET_HISTORICAL_CANDLESTICKS ||
                                message.getType() == ccapi::Message::Type::MARKET_DATA_EVENTS_CANDLESTICK) {
                                for (const auto& element : message.getElementList()) {
                                    Candle candle;
                                    candle.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        message.getTime().time_since_epoch()).count();

                                    // CCAPI normalized message time might not be the candle time?
                                    // OKX candle data: [ts, o, h, l, c, vol, ...]
                                    // CCAPI parses this.
                                    // But wait, if CCAPI parses it, message.getTime() should be the candle timestamp.

                                    candle.open = std::stod(element.getValue(CCAPI_OPEN_PRICE));
                                    candle.high = std::stod(element.getValue(CCAPI_HIGH_PRICE));
                                    candle.low = std::stod(element.getValue(CCAPI_LOW_PRICE));
                                    candle.close = std::stod(element.getValue(CCAPI_CLOSE_PRICE));
                                    candle.volume = std::stod(element.getValue(CCAPI_VOLUME));

                                    batch_candles.push_back(candle);
                                }
                                success = true;
                            } else if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                                // std::cout << "OKX Error: " << message.toString() << std::endl;
                                success = true; // Stop waiting
                            }
                        }
                    }
                }
                if (success) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (batch_candles.empty()) {
                break; // No more data
            }

            // Sort batch by timestamp
            std::sort(batch_candles.begin(), batch_candles.end(), [](const Candle& a, const Candle& b) {
                return a.timestamp < b.timestamp;
            });

            // Determine next "end" time (which is "after" parameter in OKX API terms, meaning OLDER than)
            // OKX `after` takes a timestamp and returns candles OLDER than that.
            // So we need the timestamp of the oldest candle we just got.
            int64_t oldest_ts = batch_candles.front().timestamp;

            // Prevent infinite loops if API returns same data
            if (oldest_ts >= current_to) {
                break;
            }

            all_candles.insert(all_candles.end(), batch_candles.begin(), batch_candles.end());

            current_to = oldest_ts;

            if (current_to <= from_date) break;
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

    int timeframeToSeconds(const std::string& timeframe) {
        if (timeframe == "1m") return 60;
        if (timeframe == "3m") return 180;
        if (timeframe == "5m") return 300;
        if (timeframe == "15m") return 900;
        if (timeframe == "30m") return 1800;
        if (timeframe == "1h") return 3600;
        if (timeframe == "2h") return 7200;
        if (timeframe == "4h") return 14400;
        if (timeframe == "6h") return 21600;
        if (timeframe == "12h") return 43200;
        if (timeframe == "1d") return 86400;
        if (timeframe == "1w") return 604800;
        if (timeframe == "1M") return 2592000;
        return 60;
    }

private:
    std::shared_ptr<UnifiedSession> session;
};

Okx::Okx(std::shared_ptr<UnifiedSession> session) : pimpl(std::make_unique<Impl>(session)) {}
Okx::~Okx() = default;

std::vector<Instrument> Okx::get_instruments() {
    return pimpl->get_instruments();
}

std::vector<Candle> Okx::get_historical_candles(const std::string& instrument_name,
                                                     const std::string& timeframe,
                                                     int64_t from_date,
                                                     int64_t to_date) {
    return pimpl->get_historical_candles(instrument_name, timeframe, from_date, to_date);
}

} // namespace nccapi
