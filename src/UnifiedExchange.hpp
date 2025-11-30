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

struct Trade {
    std::string id;
    std::string timestamp;
    std::string symbol;
    double price;
    double size;
    std::string side; // "BUY", "SELL", or "UNKNOWN"
    bool isBuyerMaker;
};

struct Instrument {
    std::string symbol;
    std::string baseAsset;
    std::string quoteAsset;
    // Add other fields as needed (min size, tick size)
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

        // Specialized handling for Quirky Exchanges
        if (exchangeName_ == "coinbase") {
            try {
                // Use Generic Request for Coinbase Ticker
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
        }
        else if (exchangeName_ == "okx") {
            // OKX needs 'instType' param for GET_BBOS or use Generic
            // Try Generic for safety as CCAPI might need instrument type explicitly passed
            try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/api/v5/market/ticker"},
                    {"HTTP_QUERY_STRING", "instId=" + symbol}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject() && d.HasMember("data") && d["data"].IsArray() && d["data"].Size() > 0) {
                                        const auto& data = d["data"].GetArray()[0];
                                        if (data.HasMember("bidPx")) ticker.bidPrice = std::stod(data["bidPx"].GetString());
                                        if (data.HasMember("bidSz")) ticker.bidSize = std::stod(data["bidSz"].GetString());
                                        if (data.HasMember("askPx")) ticker.askPrice = std::stod(data["askPx"].GetString());
                                        if (data.HasMember("askSz")) ticker.askSize = std::stod(data["askSz"].GetString());
                                        if (data.HasMember("last")) ticker.lastPrice = std::stod(data["last"].GetString());
                                        foundData = true;
                                    }
                                }
                            }
                        }
                    }
                }
            } catch(...) {}
        }
        else if (exchangeName_ == "kraken") {
             // Kraken REST Ticker
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/0/public/Ticker"},
                    {"HTTP_QUERY_STRING", "pair=" + symbol}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject() && d.HasMember("result")) {
                                        auto result = d["result"].GetObject();
                                        // The key in result is the pair name, might differ from symbol. Iterating first key.
                                        if (result.MemberCount() > 0) {
                                            auto pairData = result.MemberBegin()->value.GetObject();
                                            if (pairData.HasMember("b")) { ticker.bidPrice = std::stod(pairData["b"].GetArray()[0].GetString()); }
                                            if (pairData.HasMember("a")) { ticker.askPrice = std::stod(pairData["a"].GetArray()[0].GetString()); }
                                            if (pairData.HasMember("c")) { ticker.lastPrice = std::stod(pairData["c"].GetArray()[0].GetString()); }
                                            foundData = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
        }
        else if (exchangeName_ == "kucoin") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/api/v1/market/orderbook/level1"},
                    {"HTTP_QUERY_STRING", "symbol=" + symbol}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject() && d.HasMember("data")) {
                                        auto data = d["data"].GetObject();
                                        if (data.HasMember("bestBid")) ticker.bidPrice = std::stod(data["bestBid"].GetString());
                                        if (data.HasMember("bestBidSize")) ticker.bidSize = std::stod(data["bestBidSize"].GetString());
                                        if (data.HasMember("bestAsk")) ticker.askPrice = std::stod(data["bestAsk"].GetString());
                                        if (data.HasMember("bestAskSize")) ticker.askSize = std::stod(data["bestAskSize"].GetString());
                                        if (data.HasMember("price")) ticker.lastPrice = std::stod(data["price"].GetString());
                                        foundData = true;
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
        }
        else if (exchangeName_ == "gateio") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/spot/tickers"},
                    {"HTTP_QUERY_STRING", "currency_pair=" + symbol}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsArray() && d.Size() > 0) {
                                        const auto& data = d.GetArray()[0];
                                        if (data.HasMember("highest_bid")) ticker.bidPrice = std::stod(data["highest_bid"].GetString());
                                        if (data.HasMember("lowest_ask")) ticker.askPrice = std::stod(data["lowest_ask"].GetString());
                                        if (data.HasMember("last")) ticker.lastPrice = std::stod(data["last"].GetString());
                                        foundData = true;
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
        }
        else if (exchangeName_ == "bitstamp") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/api/v2/ticker/" + symbol + "/"}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject()) {
                                        if (d.HasMember("bid")) ticker.bidPrice = std::stod(d["bid"].GetString());
                                        if (d.HasMember("ask")) ticker.askPrice = std::stod(d["ask"].GetString());
                                        if (d.HasMember("last")) ticker.lastPrice = std::stod(d["last"].GetString());
                                        foundData = true;
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
        }
        else if (exchangeName_ == "gemini") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/v1/pubticker/" + symbol}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject()) {
                                        if (d.HasMember("bid")) ticker.bidPrice = std::stod(d["bid"].GetString());
                                        if (d.HasMember("ask")) ticker.askPrice = std::stod(d["ask"].GetString());
                                        if (d.HasMember("last")) ticker.lastPrice = std::stod(d["last"].GetString());
                                        foundData = true;
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
        }
        else if (exchangeName_ == "huobi") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/market/detail/merged"},
                    {"HTTP_QUERY_STRING", "symbol=" + symbol}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject() && d.HasMember("tick")) {
                                        auto tick = d["tick"].GetObject();
                                        if (tick.HasMember("bid")) ticker.bidPrice = tick["bid"].GetArray()[0].GetDouble();
                                        if (tick.HasMember("ask")) ticker.askPrice = tick["ask"].GetArray()[0].GetDouble();
                                        if (tick.HasMember("close")) ticker.lastPrice = tick["close"].GetDouble(); // Huobi uses 'close' as last
                                        foundData = true;
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
        }
        else {
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

    std::vector<Trade> fetchTrades(const std::string& symbol, int limit = 100) {
        ccapi::Request request(ccapi::Request::Operation::GET_RECENT_TRADES, exchangeName_, symbol);
        request.appendParam({
            {"LIMIT", std::to_string(limit)}
        });

        auto events = sendRequestSync(request);
        std::vector<Trade> trades;

        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                     if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                        throw std::runtime_error("Error fetching trades: " + message.toString());
                    }

                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        Trade trade;
                        trade.symbol = symbol;
                        if (map.count("TRADE_ID")) trade.id = map.at("TRADE_ID");
                        if (map.count("LAST_PRICE")) trade.price = std::stod(map.at("LAST_PRICE"));
                        else if (map.count("PRICE")) trade.price = std::stod(map.at("PRICE")); // some exchanges might use PRICE

                        if (map.count("LAST_SIZE")) trade.size = std::stod(map.at("LAST_SIZE"));
                        else if (map.count("SIZE")) trade.size = std::stod(map.at("SIZE"));

                        if (map.count("IS_BUYER_MAKER")) {
                            trade.isBuyerMaker = map.at("IS_BUYER_MAKER") == "1" || map.at("IS_BUYER_MAKER") == "true";
                            // Usually: Buyer is Maker -> Sell side trade (aggressor is seller) -> Price goes down
                            // Buyer is Taker -> Buy side trade (aggressor is buyer) -> Price goes up
                            // Standard CCXT: side is the taker's side.
                            // If isBuyerMaker is true (1), buyer is maker, so seller is taker -> "sell"
                            // If isBuyerMaker is false (0), buyer is taker -> "buy"
                            trade.side = trade.isBuyerMaker ? "sell" : "buy";
                        } else {
                            trade.side = "unknown";
                        }

                        // Timestamps
                        // CCAPI messages have `time` (exchange time) and `timeReceived`.
                        // We use the timestamp from the message itself if available (element specific?)
                        // Usually trades have a timestamp in element.
                        // CCAPI normalizes it?
                        // Let's use message timestamp as fallback or specific field.
                        // Some exchanges provide "TIMESTAMP" in element.
                        // But CCAPI message has `getTimeISO()`.
                        // Since multiple trades can be in one message or one trade per message?
                        // GET_RECENT_TRADES usually returns a list of elements in one message.
                        // Each element might NOT have a timestamp if it's shared.
                        // However, trades usually have individual timestamps.
                        // CCAPI param `GET_RECENT_TRADES` doc example shows `LAST_PRICE`, `LAST_SIZE`, `TRADE_ID`, `IS_BUYER_MAKER`.
                        // Does it show timestamp?
                        // Let's look for "TIMESTAMP" or "TIME" in map.
                        if (map.count("TIMESTAMP")) trade.timestamp = map.at("TIMESTAMP");
                        else trade.timestamp = message.getTimeISO(); // Fallback

                        trades.push_back(trade);
                    }
                }
            }
        }
        return trades;
    }

    // Experimental: Fetch Instruments (may vary heavily by exchange)
    std::vector<Instrument> fetchInstruments() {
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, exchangeName_);

        auto events = sendRequestSync(request);
        std::vector<Instrument> instruments;

        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                     if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) {
                        // Some exchanges might not support GET_INSTRUMENTS or need specific implementation.
                        // Just return empty or log.
                        std::cerr << "Warning: GET_INSTRUMENTS failed: " << message.toString() << std::endl;
                        continue;
                    }

                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        Instrument instrument;
                        if (map.count("SYMBOL")) instrument.symbol = map.at("SYMBOL");
                        if (map.count("BASE_ASSET")) instrument.baseAsset = map.at("BASE_ASSET");
                        if (map.count("QUOTE_ASSET")) instrument.quoteAsset = map.at("QUOTE_ASSET");
                        instruments.push_back(instrument);
                    }
                }
            }
        }
        return instruments;
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
        else if (exchangeName_ == "kraken") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OrderBook Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/0/public/Depth"},
                    {"HTTP_QUERY_STRING", "pair=" + symbol + "&count=" + std::to_string(limit)}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject() && d.HasMember("result")) {
                                        auto result = d["result"].GetObject();
                                        if (result.MemberCount() > 0) {
                                            auto pairData = result.MemberBegin()->value.GetObject();
                                            if (pairData.HasMember("bids") && pairData["bids"].IsArray()) {
                                                for (const auto& b : pairData["bids"].GetArray()) {
                                                    orderBook.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                                                }
                                            }
                                            if (pairData.HasMember("asks") && pairData["asks"].IsArray()) {
                                                for (const auto& a : pairData["asks"].GetArray()) {
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
             } catch(...) {}
        }
        else if (exchangeName_ == "kucoin") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OrderBook Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/api/v1/market/orderbook/level2_100"},
                    {"HTTP_QUERY_STRING", "symbol=" + symbol}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject() && d.HasMember("data")) {
                                        auto data = d["data"].GetObject();
                                        if (data.HasMember("bids") && data["bids"].IsArray()) {
                                            for (const auto& b : data["bids"].GetArray()) {
                                                orderBook.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                                            }
                                        }
                                        if (data.HasMember("asks") && data["asks"].IsArray()) {
                                            for (const auto& a : data["asks"].GetArray()) {
                                                orderBook.asks.push_back({std::stod(a[0].GetString()), std::stod(a[1].GetString())});
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
        }
        else if (exchangeName_ == "gateio") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OrderBook Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/spot/order_book"},
                    {"HTTP_QUERY_STRING", "currency_pair=" + symbol + "&limit=" + std::to_string(limit)}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
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
             } catch(...) {}
        }
        else if (exchangeName_ == "bitstamp") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OrderBook Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/api/v2/order_book/" + symbol + "/"}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
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
             } catch(...) {}
        }
        else if (exchangeName_ == "gemini") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OrderBook Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/v1/book/" + symbol}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject()) {
                                        if (d.HasMember("bids") && d["bids"].IsArray()) {
                                            for (const auto& b : d["bids"].GetArray()) {
                                                orderBook.bids.push_back({std::stod(b["price"].GetString()), std::stod(b["amount"].GetString())});
                                            }
                                        }
                                        if (d.HasMember("asks") && d["asks"].IsArray()) {
                                            for (const auto& a : d["asks"].GetArray()) {
                                                orderBook.asks.push_back({std::stod(a["price"].GetString()), std::stod(a["amount"].GetString())});
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
        }
        else if (exchangeName_ == "huobi") {
             try {
                ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OrderBook Generic");
                request.appendParam({
                    {"HTTP_METHOD", "GET"},
                    {"HTTP_PATH", "/market/depth"},
                    {"HTTP_QUERY_STRING", "symbol=" + symbol + "&type=step0"}
                });
                auto events = sendRequestSync(request);
                for (const auto& event : events) {
                    if (event.getType() == ccapi::Event::Type::RESPONSE) {
                        for (const auto& message : event.getMessageList()) {
                            for (const auto& element : message.getElementList()) {
                                if (element.getNameValueMap().count("HTTP_BODY")) {
                                    std::string body = element.getNameValueMap().at("HTTP_BODY");
                                    rapidjson::Document d;
                                    d.Parse(body.c_str());
                                    if (!d.HasParseError() && d.IsObject() && d.HasMember("tick")) {
                                        auto tick = d["tick"].GetObject();
                                        if (tick.HasMember("bids") && tick["bids"].IsArray()) {
                                            for (const auto& b : tick["bids"].GetArray()) {
                                                orderBook.bids.push_back({b[0].GetDouble(), b[1].GetDouble()});
                                            }
                                        }
                                        if (tick.HasMember("asks") && tick["asks"].IsArray()) {
                                            for (const auto& a : tick["asks"].GetArray()) {
                                                orderBook.asks.push_back({a[0].GetDouble(), a[1].GetDouble()});
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
             } catch(...) {}
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
