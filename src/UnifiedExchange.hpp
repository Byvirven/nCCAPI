#pragma once

#include "ccapi_cpp/ccapi_session.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <algorithm>
#include "rapidjson/document.h"

namespace unified_crypto {

// Normalized Data Structures
struct Ticker {
    std::string symbol;
    double lastPrice = 0.0;
    double bidPrice = 0.0;
    double bidSize = 0.0;
    double askPrice = 0.0;
    double askSize = 0.0;
    std::string timestamp; // ISO 8601
};

struct OrderBookEntry {
    double price;
    double size;
};

struct OrderBook {
    std::string symbol;
    std::vector<OrderBookEntry> bids;
    std::vector<OrderBookEntry> asks;
    std::string timestamp;
};

struct Order {
    std::string id;
    std::string symbol;
    std::string side; // "BUY" or "SELL"
    double price;
    double amount;
    std::string status;
};

struct OHLCV {
    std::string timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
};

// Configuration
struct ExchangeConfig {
    std::string apiKey;
    std::string apiSecret;
    std::string passphrase;
};

// Main Wrapper Class
class UnifiedExchange {
public:
    // Initialize static member if needed, or user does it.
    // For header-only, we might need an inline definition or just ensure the user defines it.

    UnifiedExchange(const std::string& exchange, const ExchangeConfig& config = {})
        : exchangeName_(exchange), config_(config) {

        ccapi::SessionOptions sessionOptions;
        ccapi::SessionConfigs sessionConfigs;
        // Adjust logging if needed
        // ccapi::Logger::logger = nullptr; // Disable default logging to stdout if desired

        session_ = std::make_unique<ccapi::Session>(sessionOptions, sessionConfigs);
    }

    virtual ~UnifiedExchange() {
        if (session_) {
            session_->stop();
        }
    }

    // --- Public API Implementation ---

    Ticker fetchTicker(const std::string& symbol) {
        Ticker ticker;
        ticker.symbol = symbol;
        bool foundData = false;

        // Specialized handling for Coinbase
        if (exchangeName_ == "coinbase") {
            try {
                // Use Generic Request for Coinbase Ticker
                // Endpoint: /products/{product-id}/book?level=1
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/products/" + symbol + "/book"},
                    {"HTTP_QUERY_STRING", "level=1"}
                });

                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                const auto& map = element.getNameValueMap();
                                if (map.count("HTTP_BODY")) {
                                    std::string body = map.at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject()) {
                                        if (d.HasMember("bids") && d["bids"].IsArray() && d["bids"].Size() > 0) {
                                            auto bids = d["bids"].GetArray();
                                            ticker.bidPrice = std::stod(bids[0][0].GetString());
                                            ticker.bidSize = std::stod(bids[0][1].GetString());
                                        }
                                        if (d.HasMember("asks") && d["asks"].IsArray() && d["asks"].Size() > 0) {
                                            auto asks = d["asks"].GetArray();
                                            ticker.askPrice = std::stod(asks[0][0].GetString());
                                            ticker.askSize = std::stod(asks[0][1].GetString());
                                        }
                                        foundData = true;
                                    }
                                }
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Coinbase Ticker Generic Request Failed: " << e.what() << std::endl;
            }
        } else {
            // Standard CCAPI GET_BBOS
            ccapi::Request request(ccapi::Request::Operation::GET_BBOS, exchangeName_, symbol);
            auto events = sendRequestSync(request);

            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            throw std::runtime_error("Error fetching ticker: " + message.toString());
                        }

                        ticker.timestamp = message.getTimeISO();
                        for (const auto& element : message.getElementList()) {
                            const auto& map = element.getNameValueMap();

                            // Filter by instrument if present (GET_BBOS on Binance returns all tickers)
                            if (map.count("INSTRUMENT") && map.at("INSTRUMENT") != symbol) {
                                continue;
                            }

                            if (map.count("BID_PRICE")) { ticker.bidPrice = std::stod(map.at("BID_PRICE")); foundData = true; }
                            if (map.count("BID_SIZE")) ticker.bidSize = std::stod(map.at("BID_SIZE"));
                            if (map.count("ASK_PRICE")) { ticker.askPrice = std::stod(map.at("ASK_PRICE")); foundData = true; }
                            if (map.count("ASK_SIZE")) ticker.askSize = std::stod(map.at("ASK_SIZE"));
                        }
                    }
                }
            }
        }

        if (!foundData) {
            std::cerr << "Warning: No Ticker data found in response for " << symbol << " on " << exchangeName_ << std::endl;
        }

        if (ticker.bidPrice > 0 && ticker.askPrice > 0) {
            ticker.lastPrice = (ticker.bidPrice + ticker.askPrice) / 2.0;
        }

        return ticker;
    }

    std::vector<OHLCV> fetchOHLCV(const std::string& symbol, const std::string& timeframe = "60", int limit = 100) {
        // timeframe in seconds for CCAPI standard, usually.
        ccapi::Request request(ccapi::Request::Operation::GET_RECENT_CANDLESTICKS, exchangeName_, symbol);
        request.appendParam({
            {"CANDLESTICK_INTERVAL_SECONDS", timeframe},
            {"LIMIT", std::to_string(limit)}
        });

        auto events = sendRequestSync(request);
        std::vector<OHLCV> candles;

        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                     if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                        throw std::runtime_error("Error fetching OHLCV: " + message.toString());
                    }

                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        OHLCV candle;
                        if (map.count("START_TIME")) candle.timestamp = map.at("START_TIME");
                        if (map.count("OPEN_PRICE")) candle.open = std::stod(map.at("OPEN_PRICE"));
                        if (map.count("HIGH_PRICE")) candle.high = std::stod(map.at("HIGH_PRICE"));
                        if (map.count("LOW_PRICE")) candle.low = std::stod(map.at("LOW_PRICE"));
                        if (map.count("CLOSE_PRICE")) candle.close = std::stod(map.at("CLOSE_PRICE"));
                        if (map.count("VOLUME")) candle.volume = std::stod(map.at("VOLUME"));

                        candles.push_back(candle);
                    }
                }
            }
        }
        return candles;
    }

    OrderBook fetchOrderBook(const std::string& symbol, int limit = 10) {
        OrderBook orderBook;
        orderBook.symbol = symbol;

        if (exchangeName_ == "coinbase") {
             // Coinbase specific via Generic Request
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OrderBook Generic");
             request.appendParam({
                {"HTTP_METHOD", "GET"},
                {"HTTP_PATH", "/products/" + symbol + "/book"},
                {"HTTP_QUERY_STRING", "level=2"}
             });
             auto events = sendRequestSync(request);
             // Parse logic for Coinbase generic response (similar to Ticker but parsing arrays of bids/asks)
             for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        for (const auto& element : message.getElementList()) {
                            const auto& map = element.getNameValueMap();
                            if (map.count("HTTP_BODY")) {
                                std::string body = map.at("HTTP_BODY");
                                rapidjson::Document d;
                                d.Parse(body.c_str());
                                if (!d.HasParseError() && d.IsObject()) {
                                    if (d.HasMember("bids") && d["bids"].IsArray()) {
                                        for (const auto& b : d["bids"].GetArray()) {
                                            orderBook.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                                            if (orderBook.bids.size() >= limit) break;
                                        }
                                    }
                                    if (d.HasMember("asks") && d["asks"].IsArray()) {
                                        for (const auto& a : d["asks"].GetArray()) {
                                            orderBook.asks.push_back({std::stod(a[0].GetString()), std::stod(a[1].GetString())});
                                            if (orderBook.asks.size() >= limit) break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
             }
        }
        else if (exchangeName_ == "binance-us") {
             // BinanceUS specific via Generic Request to bypass CCAPI bug
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OrderBook Generic");
             request.appendParam({
                {"HTTP_METHOD", "GET"},
                {"HTTP_PATH", "/api/v3/depth"},
                {"HTTP_QUERY_STRING", "symbol=" + symbol + "&limit=" + std::to_string(limit)}
             });
             auto events = sendRequestSync(request);
             for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        for (const auto& element : message.getElementList()) {
                            const auto& map = element.getNameValueMap();
                            if (map.count("HTTP_BODY")) {
                                std::string body = map.at("HTTP_BODY");
                                rapidjson::Document d;
                                d.Parse(body.c_str());
                                if (!d.HasParseError() && d.IsObject()) {
                                    if (d.HasMember("bids") && d["bids"].IsArray()) {
                                        for (const auto& b : d["bids"].GetArray()) {
                                            orderBook.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                                        }
                                    }
                                    if (d.HasMember("asks") && d["asks"].IsArray()) {
                                        for (const auto& a : d["asks"].GetArray()) {
                                            orderBook.asks.push_back({std::stod(a[0].GetString()), std::stod(a[1].GetString())});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
             }
        }
        else {
            // Standard Path
            ccapi::Request request(ccapi::Request::Operation::GET_MARKET_DEPTH, exchangeName_, symbol);
            request.appendParam({
                {"LIMIT", std::to_string(limit)}
            });

            auto events = sendRequestSync(request);
            for (const auto& event : events) {
                if (event.getType() == ccapi::Event::Type::RESPONSE) {
                    for (const auto& message : event.getMessageList()) {
                        if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                            throw std::runtime_error("Error fetching orderbook: " + message.toString());
                        }

                        orderBook.timestamp = message.getTimeISO();
                        for (const auto& element : message.getElementList()) {
                            const auto& map = element.getNameValueMap();
                            if (map.count("BID_PRICE") && map.count("BID_SIZE")) {
                                orderBook.bids.push_back({std::stod(map.at("BID_PRICE")), std::stod(map.at("BID_SIZE"))});
                            }
                            if (map.count("ASK_PRICE") && map.count("ASK_SIZE")) {
                                orderBook.asks.push_back({std::stod(map.at("ASK_PRICE")), std::stod(map.at("ASK_SIZE"))});
                            }
                        }
                    }
                }
            }
        }
        return orderBook;
    }

    // --- Private API Implementation ---

    // Returns Order ID
    std::string createOrder(const std::string& symbol, const std::string& side, double amount, double price = 0.0) {
        ccapi::Request request(ccapi::Request::Operation::CREATE_ORDER, exchangeName_, symbol);

        // Normalize Side
        std::string sideUpper = side;
        std::transform(sideUpper.begin(), sideUpper.end(), sideUpper.begin(), ::toupper);
        request.appendParam({
            {"SIDE", sideUpper},
            {"QUANTITY", std::to_string(amount)}
        });

        if (price > 0.0) {
            request.appendParam({
                {"LIMIT_PRICE", std::to_string(price)},
                {"ORDER_TYPE", "LIMIT"} // Usually required if price is present
            });
        } else {
            request.appendParam({
                {"ORDER_TYPE", "MARKET"}
            });
        }

        // Add Credentials
        if (!config_.apiKey.empty()) {
            std::map<std::string, std::string> creds;
            creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
            creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
            if (!config_.passphrase.empty()) {
                creds[ccapi::toString(exchangeName_) + "_API_PASSPHRASE"] = config_.passphrase;
            }
            request.setCredential(creds);
        } else {
            throw std::runtime_error("API Key required for createOrder");
        }

        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                        throw std::runtime_error("Create Order Error: " + message.toString());
                    }
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        if (map.count("ORDER_ID")) return map.at("ORDER_ID");
                        if (map.count("CLIENT_ORDER_ID")) return map.at("CLIENT_ORDER_ID");
                    }
                }
            }
        }
        throw std::runtime_error("No Order ID returned");
    }

    // Returns map of currency -> available balance
    std::map<std::string, double> fetchBalance() {
        // GET_ACCOUNTS usually returns balances
        ccapi::Request request(ccapi::Request::Operation::GET_ACCOUNTS, exchangeName_);

        if (!config_.apiKey.empty()) {
            std::map<std::string, std::string> creds;
            creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
            creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
            if (!config_.passphrase.empty()) {
                creds[ccapi::toString(exchangeName_) + "_API_PASSPHRASE"] = config_.passphrase;
            }
            request.setCredential(creds);
        } else {
            throw std::runtime_error("API Key required for fetchBalance");
        }

        auto events = sendRequestSync(request);
        std::map<std::string, double> balances;

        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                     if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                        throw std::runtime_error("Fetch Balance Error: " + message.toString());
                    }
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        std::string asset;
                        double free = 0.0;

                        // CCAPI Normalization for Balances?
                        // Usually: ASSET, FREE, LOCKED or AVAILABLE, HOLD
                        // Binance: ASSET, FREE, LOCKED
                        // Coinbase: CURRENCY, AVAILABLE, HOLD

                        if (map.count("ASSET")) asset = map.at("ASSET");
                        else if (map.count("CURRENCY")) asset = map.at("CURRENCY");

                        if (map.count("FREE")) free = std::stod(map.at("FREE"));
                        else if (map.count("AVAILABLE")) free = std::stod(map.at("AVAILABLE"));

                        if (!asset.empty()) {
                            balances[asset] = free;
                        }
                    }
                }
            }
        }
        return balances;
    }

private:
    std::string exchangeName_;
    ExchangeConfig config_;
    std::unique_ptr<ccapi::Session> session_;

    std::vector<ccapi::Event> sendRequestSync(ccapi::Request request) {
        // Ensure correlationId is set to track the response
        if (request.getCorrelationId().empty()) {
            request.setCorrelationId(std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        }
        std::string correlationId = request.getCorrelationId();

        ccapi::Queue<ccapi::Event> eventQueue;
        session_->sendRequest(request, &eventQueue);

        std::vector<ccapi::Event> accumulatedEvents;
        int timeoutMs = 10000;
        int elapsed = 0;
        int pollInterval = 10;
        bool responseReceived = false;

        while (elapsed < timeoutMs && !responseReceived) {
            if (!eventQueue.empty()) {
                std::vector<ccapi::Event> batch = eventQueue.purge();
                for (const auto& event : batch) {
                    accumulatedEvents.push_back(event);
                    if (event.getType() == ccapi::Event::Type::RESPONSE ||
                        event.getType() == ccapi::Event::Type::REQUEST_STATUS) {

                        // Check correlation ID in messages
                        for (const auto& msg : event.getMessageList()) {
                            const auto& idList = msg.getCorrelationIdList();
                            if (std::find(idList.begin(), idList.end(), correlationId) != idList.end()) {
                                responseReceived = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (!responseReceived) {
                std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
                elapsed += pollInterval;
            }
        }

        if (!responseReceived) {
            std::cerr << "Timeout or no correlation match for " << exchangeName_ << " ReqID: " << correlationId << std::endl;
        }

        return accumulatedEvents;
    }
};

} // namespace unified_crypto
