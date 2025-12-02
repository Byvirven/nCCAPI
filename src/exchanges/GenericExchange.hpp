#pragma once

#include "../Exchange.hpp"
#include "../ccapi_config.hpp"
#include "ccapi_cpp/ccapi_session.h"
#include "rapidjson/document.h"
#include <memory>
#include <iostream>
#include <thread>
#include <algorithm>
#include <mutex>
#include <chrono>

namespace unified_crypto {

/**
 * @class GenericExchange
 * @brief A generic implementation of the Exchange interface using CCAPI.
 */
class GenericExchange : public Exchange {
protected:
    class GenericEventHandler;

    std::string exchangeName_;
    ExchangeConfig config_;

    TickerCallback onTicker_;
    OrderBookCallback onOrderBook_;
    TradeCallback onTrade_;
    OHLCVCallback onOHLCV_;
    OrderUpdateCallback onOrderUpdate_;
    AccountUpdateCallback onAccountUpdate_;

    std::unique_ptr<GenericEventHandler> eventHandler_;
    std::unique_ptr<ccapi::Session> session_;

    std::vector<ccapi::Event> sendRequestSync(ccapi::Request request) {
        if (request.getCorrelationId().empty()) {
            request.setCorrelationId(std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        }
        ccapi::Queue<ccapi::Event> eventQueue;
        session_->sendRequest(request, &eventQueue);

        std::vector<ccapi::Event> accumulatedEvents;
        int timeoutMs = 15000;
        int elapsed = 0;
        bool responseReceived = false;

        while (elapsed < timeoutMs && !responseReceived) {
            if (!eventQueue.empty()) {
                std::vector<ccapi::Event> batch = eventQueue.purge();
                for (const auto& event : batch) {
                    accumulatedEvents.push_back(event);
                    if (event.getType() == ccapi::Event::Type::RESPONSE) responseReceived = true;
                }
            }
            if (!responseReceived) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                elapsed += 10;
            }
        }
        return accumulatedEvents;
    }

    // Helpers
    std::string isoToMs(const std::string& iso) {
        try {
            auto tp = ccapi::UtilTime::parse(iso);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
            return std::to_string(ms);
        } catch (...) {
            return "0";
        }
    }

    std::string msToIso(long long ms) {
        return ccapi::UtilTime::getISOTimestamp(ccapi::UtilTime::makeTimePointFromMilliseconds(ms));
    }

    // Quirks
    bool shouldUseGenericTicker() {
        static std::vector<std::string> quirks = {"coinbase", "okx", "cryptocom", "deribit", "kraken", "kucoin", "bitstamp", "gemini", "huobi", "bitfinex", "ascendex", "bybit", "mexc", "whitebit"};
        return std::find(quirks.begin(), quirks.end(), exchangeName_) != quirks.end() || exchangeName_.find("mexc") != std::string::npos;
    }
    bool shouldUseGenericOrderBook() {
        static std::vector<std::string> quirks = {"coinbase", "binance-us", "binance", "kraken", "kucoin", "gateio", "bitstamp", "gemini", "huobi", "bitfinex", "ascendex", "bybit", "mexc", "cryptocom", "deribit"};
        return std::find(quirks.begin(), quirks.end(), exchangeName_) != quirks.end() || exchangeName_.find("binance") != std::string::npos || exchangeName_.find("gateio") != std::string::npos;
    }
    bool shouldUseGenericOHLCV() {
        static std::vector<std::string> quirks = {"bybit", "gateio", "whitebit", "bitmex", "ascendex"};
        return std::find(quirks.begin(), quirks.end(), exchangeName_) != quirks.end() || exchangeName_.find("gateio") != std::string::npos;
    }
    bool shouldUseGenericInstruments() {
        static std::vector<std::string> quirks = {"bybit", "gateio", "deribit", "whitebit", "okx", "binance-us", "binance", "ascendex"};
        return std::find(quirks.begin(), quirks.end(), exchangeName_) != quirks.end() || exchangeName_.find("gateio") != std::string::npos;
    }
    bool shouldUseGenericInstrument() {
        return shouldUseGenericInstruments();
    }
    bool shouldUseGenericTrades() {
         static std::vector<std::string> quirks = {"ascendex"};
         return std::find(quirks.begin(), quirks.end(), exchangeName_) != quirks.end();
    }

    // Generic Implementations
    Ticker fetchTickerGeneric(const std::string& symbol) {
         Ticker ticker; ticker.symbol = symbol;
         ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Ticker Generic");

         if (exchangeName_ == "coinbase") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/products/" + symbol + "/book"}, {"HTTP_QUERY_STRING", "level=1"}});
         } else if (exchangeName_ == "bybit") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/v5/market/tickers"}, {"HTTP_QUERY_STRING", "category=spot&symbol=" + symbol}});
         } else if (exchangeName_ == "gateio" || exchangeName_.find("gateio") != std::string::npos) {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/spot/tickers"}, {"HTTP_QUERY_STRING", "currency_pair=" + symbol}});
         } else if (exchangeName_ == "kraken") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/0/public/Ticker"}, {"HTTP_QUERY_STRING", "pair=" + symbol}});
         } else if (exchangeName_ == "ascendex") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/spot/ticker"}, {"HTTP_QUERY_STRING", "symbol=" + symbol}});
         }

         auto events = sendRequestSync(request);
         for(const auto& event : events) {
             for(const auto& msg : event.getMessageList()) {
                 for(const auto& elem : msg.getElementList()) {
                     if(elem.getNameValueMap().count("HTTP_BODY")) {
                         rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                         if(!d.HasParseError()) {
                             if(exchangeName_ == "coinbase" && d.HasMember("bids")) {
                                 ticker.bidPrice = std::stod(d["bids"].GetArray()[0][0].GetString());
                                 ticker.askPrice = std::stod(d["asks"].GetArray()[0][0].GetString());
                             } else if (exchangeName_ == "bybit" && d.HasMember("result")) {
                                 const auto& l = d["result"]["list"].GetArray()[0];
                                 ticker.bidPrice = std::stod(l["bid1Price"].GetString());
                                 ticker.askPrice = std::stod(l["ask1Price"].GetString());
                                 ticker.lastPrice = std::stod(l["lastPrice"].GetString());
                             } else if (exchangeName_ == "ascendex" && d.HasMember("data")) {
                                 const auto& data = d["data"];
                                 if(data.IsObject()) {
                                     ticker.lastPrice = std::stod(data["close"].GetString());
                                     ticker.bidPrice = std::stod(data["bid"].GetArray()[0].GetString());
                                     ticker.bidSize = std::stod(data["bid"].GetArray()[1].GetString());
                                     ticker.askPrice = std::stod(data["ask"].GetArray()[0].GetString());
                                     ticker.askSize = std::stod(data["ask"].GetArray()[1].GetString());
                                 }
                             }
                         }
                     }
                 }
             }
         }
         if (ticker.lastPrice == 0.0 && ticker.bidPrice > 0) ticker.lastPrice = (ticker.bidPrice + ticker.askPrice)/2.0;
         return ticker;
    }

    OrderBook fetchOrderBookGeneric(const std::string& symbol, int limit) {
        OrderBook ob; ob.symbol = symbol;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Book Generic");

        if (exchangeName_ == "gateio") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/spot/order_book"}, {"HTTP_QUERY_STRING", "currency_pair=" + symbol}});
        } else if (exchangeName_ == "bybit") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/v5/market/orderbook"}, {"HTTP_QUERY_STRING", "category=spot&symbol=" + symbol}});
        } else if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/depth"}, {"HTTP_QUERY_STRING", "symbol=" + symbol + "&limit=" + std::to_string(limit)}});
        } else if (exchangeName_ == "ascendex") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/depth"}, {"HTTP_QUERY_STRING", "symbol=" + symbol}});
        }

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                         if(!d.HasParseError()) {
                             if(exchangeName_ == "gateio" && d.HasMember("bids")) {
                                 for(const auto& b : d["bids"].GetArray()) ob.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                                 for(const auto& a : d["asks"].GetArray()) ob.asks.push_back({std::stod(a[0].GetString()), std::stod(a[1].GetString())});
                             }
                             if(exchangeName_ == "bybit" && d.HasMember("result")) {
                                  for(const auto& b : d["result"]["b"].GetArray()) ob.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                                  for(const auto& a : d["result"]["a"].GetArray()) ob.asks.push_back({std::stod(a[0].GetString()), std::stod(a[1].GetString())});
                             }
                             if ((exchangeName_ == "binance-us" || exchangeName_ == "binance") && d.HasMember("bids")) {
                                 for(const auto& b : d["bids"].GetArray()) ob.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                                 for(const auto& a : d["asks"].GetArray()) ob.asks.push_back({std::stod(a[0].GetString()), std::stod(a[1].GetString())});
                             }
                             if (exchangeName_ == "ascendex" && d.HasMember("data")) {
                                 const auto& data = d["data"]["data"];
                                 ob.timestamp = msToIso(data["ts"].GetInt64());
                                 for(const auto& b : data["bids"].GetArray()) ob.bids.push_back({std::stod(b[0].GetString()), std::stod(b[1].GetString())});
                                 for(const auto& a : data["asks"].GetArray()) ob.asks.push_back({std::stod(a[0].GetString()), std::stod(a[1].GetString())});
                             }
                         }
                    }
                }
            }
        }
        return ob;
    }

    std::vector<OHLCV> fetchOHLCVGeneric(const std::string& symbol, const std::string& timeframe, int limit) {
         std::vector<OHLCV> candles;
         ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get OHLCV Generic");

         if (exchangeName_ == "bybit") {
             std::string interval = "60";
             if (timeframe == "60") interval = "1";
             if (timeframe == "3600") interval = "60";
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/v5/market/kline"},
                {"HTTP_QUERY_STRING", "category=spot&symbol=" + symbol + "&interval=" + interval + "&limit=" + std::to_string(limit)}});
         } else if (exchangeName_ == "gateio" || exchangeName_.find("gateio") != std::string::npos) {
              std::string interval = "1m";
              if (timeframe == "60") interval = "1m";
              if (timeframe == "3600") interval = "1h";
              request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/spot/candlesticks"},
                {"HTTP_QUERY_STRING", "currency_pair=" + symbol + "&interval=" + interval + "&limit=" + std::to_string(limit)}});
         } else if (exchangeName_ == "ascendex") {
             std::string interval = "1"; // 1 minute
             if (timeframe == "3600") interval = "60";
             else if (timeframe == "86400") interval = "1d";
             // AscendEX expects interval in minutes (1, 5, 15, 60...)
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/barhist"},
                {"HTTP_QUERY_STRING", "symbol=" + symbol + "&interval=" + interval + "&n=" + std::to_string(limit)}});
         }

         auto events = sendRequestSync(request);
         for(const auto& event : events) {
             for(const auto& msg : event.getMessageList()) {
                 for(const auto& elem : msg.getElementList()) {
                     if(elem.getNameValueMap().count("HTTP_BODY")) {
                         rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                         if(!d.HasParseError()) {
                             if (exchangeName_ == "bybit" && d.HasMember("result")) {
                                 for(const auto& k : d["result"]["list"].GetArray()) {
                                     OHLCV c;
                                     c.symbol = symbol;
                                     c.timestamp = k[0].GetString();
                                     c.open = std::stod(k[1].GetString());
                                     c.high = std::stod(k[2].GetString());
                                     c.low = std::stod(k[3].GetString());
                                     c.close = std::stod(k[4].GetString());
                                     c.volume = std::stod(k[5].GetString());
                                     candles.push_back(c);
                                 }
                             }
                             if (exchangeName_.find("gateio") != std::string::npos && d.IsArray()) {
                                 for(const auto& k : d.GetArray()) {
                                     OHLCV c;
                                     c.symbol = symbol;
                                     c.timestamp = k[0].GetString();
                                     c.volume = std::stod(k[1].GetString());
                                     c.close = std::stod(k[2].GetString());
                                     c.high = std::stod(k[3].GetString());
                                     c.low = std::stod(k[4].GetString());
                                     c.open = std::stod(k[5].GetString());
                                     candles.push_back(c);
                                 }
                             }
                             if (exchangeName_ == "ascendex" && d.HasMember("data")) {
                                 for(const auto& item : d["data"].GetArray()) {
                                     const auto& k = item["data"];
                                     OHLCV c;
                                     c.symbol = symbol;
                                     c.timestamp = msToIso(k["ts"].GetInt64());
                                     c.open = std::stod(k["o"].GetString());
                                     c.high = std::stod(k["h"].GetString());
                                     c.low = std::stod(k["l"].GetString());
                                     c.close = std::stod(k["c"].GetString());
                                     c.volume = std::stod(k["v"].GetString());
                                     candles.push_back(c);
                                 }
                             }
                         }
                     }
                 }
             }
         }
         return candles;
    }

    std::vector<Instrument> fetchInstrumentsGeneric() {
        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Instruments Generic");

        if (exchangeName_ == "bybit") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/v5/market/instruments-info"}, {"HTTP_QUERY_STRING", "category=spot"}});
        } else if (exchangeName_ == "gateio" || exchangeName_.find("gateio") != std::string::npos) {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/spot/currency_pairs"}});
        } else if (exchangeName_ == "okx") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v5/public/instruments"}, {"HTTP_QUERY_STRING", "instType=SPOT"}});
        } else if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/exchangeInfo"}});
        } else if (exchangeName_ == "ascendex") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/spot/products"}});
        }

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
             if (event.getType() == ccapi::Event::Type::RESPONSE) {
                 for(const auto& msg : event.getMessageList()) {
                     for(const auto& elem : msg.getElementList()) {
                         if(elem.getNameValueMap().count("HTTP_BODY")) {
                             std::string body = elem.getNameValueMap().at("HTTP_BODY");
                             rapidjson::Document d; d.Parse(body.c_str());
                             if(!d.HasParseError()) {
                                 if (exchangeName_ == "bybit" && d.HasMember("result")) {
                                     for(const auto& i : d["result"]["list"].GetArray()) {
                                         Instrument inst;
                                         inst.symbol = i["symbol"].GetString();
                                         inst.baseAsset = i["baseCoin"].GetString();
                                         inst.quoteAsset = i["quoteCoin"].GetString();
                                         instruments.push_back(inst);
                                     }
                                 }
                                 if (exchangeName_.find("gateio") != std::string::npos && d.IsArray()) {
                                     for(const auto& i : d.GetArray()) {
                                         Instrument inst;
                                         inst.symbol = i["id"].GetString();
                                         inst.baseAsset = i["base"].GetString();
                                         inst.quoteAsset = i["quote"].GetString();
                                         instruments.push_back(inst);
                                     }
                                 }
                                 if (exchangeName_ == "okx" && d.HasMember("data")) {
                                     for(const auto& i : d["data"].GetArray()) {
                                         Instrument inst;
                                         inst.symbol = i["instId"].GetString();
                                         inst.baseAsset = i["baseCcy"].GetString();
                                         inst.quoteAsset = i["quoteCcy"].GetString();
                                         instruments.push_back(inst);
                                     }
                                 }
                                 if ((exchangeName_ == "binance-us" || exchangeName_ == "binance") && d.HasMember("symbols")) {
                                     for(const auto& i : d["symbols"].GetArray()) {
                                         Instrument inst;
                                         inst.symbol = i["symbol"].GetString();
                                         inst.baseAsset = i["baseAsset"].GetString();
                                         inst.quoteAsset = i["quoteAsset"].GetString();
                                         if (i.HasMember("status")) inst.status = i["status"].GetString();
                                         if (i.HasMember("permissions") && i["permissions"].IsArray()) {
                                             std::string perms;
                                             for(const auto& p : i["permissions"].GetArray()) {
                                                 if(!perms.empty()) perms += ",";
                                                 perms += p.GetString();
                                             }
                                             inst.type = perms;
                                         }
                                         if (i.HasMember("filters") && i["filters"].IsArray()) {
                                             for(const auto& f : i["filters"].GetArray()) {
                                                 std::string ft = f["filterType"].GetString();
                                                 if (ft == "PRICE_FILTER") inst.tickSize = std::stod(f["tickSize"].GetString());
                                                 else if (ft == "LOT_SIZE") {
                                                     inst.minSize = std::stod(f["minQty"].GetString());
                                                     inst.stepSize = std::stod(f["stepSize"].GetString());
                                                 }
                                             }
                                         }
                                         instruments.push_back(inst);
                                     }
                                 }
                                 if (exchangeName_ == "ascendex" && d.HasMember("data")) {
                                     for(const auto& i : d["data"].GetArray()) {
                                         Instrument inst;
                                         inst.symbol = i["symbol"].GetString();
                                         inst.baseAsset = i["baseAsset"].GetString();
                                         inst.quoteAsset = i["quoteAsset"].GetString();
                                         inst.status = i["statusCode"].GetString();
                                         if(i.HasMember("tickSize")) inst.tickSize = std::stod(i["tickSize"].GetString());
                                         if(i.HasMember("minQty")) inst.minSize = std::stod(i["minQty"].GetString());
                                         if(i.HasMember("lotSize")) inst.stepSize = std::stod(i["lotSize"].GetString());
                                         instruments.push_back(inst);
                                     }
                                 }
                             }
                         }
                     }
                 }
             }
        }
        return instruments;
    }

    Instrument fetchInstrumentGeneric(const std::string& symbol) {
         Instrument inst; inst.symbol = symbol;
         ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Instrument Generic");

         if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/exchangeInfo"}, {"HTTP_QUERY_STRING", "symbol=" + symbol}});
         } else if (exchangeName_ == "ascendex") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/spot/products"}});
         }

         auto events = sendRequestSync(request);
         for(const auto& event : events) {
             if (event.getType() == ccapi::Event::Type::RESPONSE) {
                 for(const auto& msg : event.getMessageList()) {
                     for(const auto& elem : msg.getElementList()) {
                         if(elem.getNameValueMap().count("HTTP_BODY")) {
                             rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                             if(!d.HasParseError()) {
                                 if ((exchangeName_ == "binance-us" || exchangeName_ == "binance") && d.HasMember("symbols")) {
                                     for(const auto& i : d["symbols"].GetArray()) {
                                         inst.baseAsset = i["baseAsset"].GetString();
                                         inst.quoteAsset = i["quoteAsset"].GetString();
                                         if (i.HasMember("status")) inst.status = i["status"].GetString();
                                         if (i.HasMember("permissions") && i["permissions"].IsArray()) {
                                             std::string perms;
                                             for(const auto& p : i["permissions"].GetArray()) {
                                                 if(!perms.empty()) perms += ",";
                                                 perms += p.GetString();
                                             }
                                             inst.type = perms;
                                         }
                                         if (i.HasMember("filters") && i["filters"].IsArray()) {
                                             for(const auto& f : i["filters"].GetArray()) {
                                                 std::string ft = f["filterType"].GetString();
                                                 if (ft == "PRICE_FILTER") inst.tickSize = std::stod(f["tickSize"].GetString());
                                                 else if (ft == "LOT_SIZE") {
                                                     inst.minSize = std::stod(f["minQty"].GetString());
                                                     inst.stepSize = std::stod(f["stepSize"].GetString());
                                                 }
                                             }
                                         }
                                     }
                                 }
                                 if (exchangeName_ == "ascendex" && d.HasMember("data")) {
                                     for(const auto& i : d["data"].GetArray()) {
                                         if(i["symbol"].GetString() == symbol) {
                                            inst.baseAsset = i["baseAsset"].GetString();
                                            inst.quoteAsset = i["quoteAsset"].GetString();
                                            inst.status = i["statusCode"].GetString();
                                            if(i.HasMember("tickSize")) inst.tickSize = std::stod(i["tickSize"].GetString());
                                            if(i.HasMember("minQty")) inst.minSize = std::stod(i["minQty"].GetString());
                                            if(i.HasMember("lotSize")) inst.stepSize = std::stod(i["lotSize"].GetString());
                                            break;
                                         }
                                     }
                                 }
                             }
                         }
                     }
                 }
             }
         }
         return inst;
    }

public:
    GenericExchange(const std::string& exchange, const ExchangeConfig& config = {})
        : exchangeName_(exchange), config_(config) {

        ccapi::SessionOptions sessionOptions;
        ccapi::SessionConfigs sessionConfigs;
        eventHandler_ = std::make_unique<GenericEventHandler>(this);
        session_ = std::make_unique<ccapi::Session>(sessionOptions, sessionConfigs, eventHandler_.get());
    }

    virtual ~GenericExchange() {
        if (session_) session_->stop();
    }

    // Callbacks
    void setOnTicker(TickerCallback cb) override { onTicker_ = cb; }
    void setOnOrderBook(OrderBookCallback cb) override { onOrderBook_ = cb; }
    void setOnTrade(TradeCallback cb) override { onTrade_ = cb; }
    void setOnOHLCV(OHLCVCallback cb) override { onOHLCV_ = cb; }
    void setOnOrderUpdate(OrderUpdateCallback cb) override { onOrderUpdate_ = cb; }
    void setOnAccountUpdate(AccountUpdateCallback cb) override { onAccountUpdate_ = cb; }

    // WebSocket - Public
    void subscribeTicker(const std::string& symbol) override {
        ccapi::Subscription subscription(exchangeName_, symbol, "MARKET_TICKER");
        session_->subscribe(subscription);
    }

    void subscribeOrderBook(const std::string& symbol, int depth = 10) override {
        std::string options = "MARKET_DEPTH_MAX=" + std::to_string(depth);
        ccapi::Subscription subscription(exchangeName_, symbol, "MARKET_DEPTH", options);
        session_->subscribe(subscription);
    }

    void subscribeTrades(const std::string& symbol) override {
        ccapi::Subscription subscription(exchangeName_, symbol, "TRADE");
        session_->subscribe(subscription);
    }

    void subscribeOHLCV(const std::string& symbol, const std::string& interval = "60") override {
        std::string options = "CANDLESTICK_INTERVAL_SECONDS=" + interval;
        ccapi::Subscription subscription(exchangeName_, symbol, "CANDLESTICK", options);
        session_->subscribe(subscription);
    }

    // WebSocket - Private
    void subscribeOrderUpdates(const std::string& symbol) override {
        ccapi::Subscription subscription(exchangeName_, symbol, "ORDER_UPDATE");
        session_->subscribe(subscription);
    }

    void subscribeAccountUpdates() override {
        ccapi::Subscription subscription(exchangeName_, "", "BALANCE_UPDATE");
        session_->subscribe(subscription);
    }

    // REST - Public
    Ticker fetchTicker(const std::string& symbol) override {
        if (shouldUseGenericTicker()) return fetchTickerGeneric(symbol);

        Ticker ticker; ticker.symbol = symbol;
        ccapi::Request request(ccapi::Request::Operation::GET_BBOS, exchangeName_, symbol);
        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) continue;
                    ticker.timestamp = message.getTimeISO();
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        if (map.count("INSTRUMENT") && map.at("INSTRUMENT") != symbol) continue;
                        if (map.count("BID_PRICE")) ticker.bidPrice = std::stod(map.at("BID_PRICE"));
                        if (map.count("BID_SIZE")) ticker.bidSize = std::stod(map.at("BID_SIZE"));
                        if (map.count("ASK_PRICE")) ticker.askPrice = std::stod(map.at("ASK_PRICE"));
                        if (map.count("ASK_SIZE")) ticker.askSize = std::stod(map.at("ASK_SIZE"));
                    }
                }
            }
        }
        if (ticker.lastPrice == 0.0 && ticker.bidPrice > 0) ticker.lastPrice = (ticker.bidPrice + ticker.askPrice)/2.0;
        return ticker;
    }

    OrderBook fetchOrderBook(const std::string& symbol, int limit = 10) override {
        if (shouldUseGenericOrderBook()) return fetchOrderBookGeneric(symbol, limit);

        OrderBook orderBook; orderBook.symbol = symbol;
        ccapi::Request request(ccapi::Request::Operation::GET_MARKET_DEPTH, exchangeName_, symbol);
        request.appendParam({{"LIMIT", std::to_string(limit)}});
        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) continue;
                    orderBook.timestamp = message.getTimeISO();
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        if (map.count("BID_PRICE") && map.count("BID_SIZE")) orderBook.bids.push_back({std::stod(map.at("BID_PRICE")), std::stod(map.at("BID_SIZE"))});
                        if (map.count("ASK_PRICE") && map.count("ASK_SIZE")) orderBook.asks.push_back({std::stod(map.at("ASK_PRICE")), std::stod(map.at("ASK_SIZE"))});
                    }
                }
            }
        }
        return orderBook;
    }

    std::vector<Trade> fetchTradesGeneric(const std::string& symbol, int limit) {
        std::vector<Trade> trades;
        ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Trades Generic");

        if (exchangeName_ == "ascendex") {
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/pro/v1/trades"}, {"HTTP_QUERY_STRING", "symbol=" + symbol + "&n=" + std::to_string(limit)}});
        }

        auto events = sendRequestSync(request);
        for(const auto& event : events) {
            for(const auto& msg : event.getMessageList()) {
                for(const auto& elem : msg.getElementList()) {
                    if(elem.getNameValueMap().count("HTTP_BODY")) {
                        rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                         if(!d.HasParseError()) {
                             if(exchangeName_ == "ascendex" && d.HasMember("data")) {
                                 const auto& data = d["data"]["data"];
                                 if(data.IsArray()) {
                                     for(const auto& t_json : data.GetArray()) {
                                         Trade t;
                                         t.symbol = symbol;
                                         t.id = std::to_string(t_json["seqnum"].GetInt64());
                                         t.price = std::stod(t_json["p"].GetString());
                                         t.size = std::stod(t_json["q"].GetString());
                                         t.timestamp = msToIso(t_json["ts"].GetInt64());
                                         t.isBuyerMaker = t_json["bm"].GetBool();
                                         t.side = t.isBuyerMaker ? "sell" : "buy";
                                         trades.push_back(t);
                                     }
                                 }
                             }
                         }
                    }
                }
            }
        }
        return trades;
    }

    std::vector<Trade> fetchTrades(const std::string& symbol, int limit = 100) override {
        if (shouldUseGenericTrades()) return fetchTradesGeneric(symbol, limit);

        std::vector<Trade> trades;
        ccapi::Request request(ccapi::Request::Operation::GET_RECENT_TRADES, exchangeName_, symbol);
        request.appendParam({{"LIMIT", std::to_string(limit)}});
        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) continue;
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        Trade t; t.symbol = symbol;
                        if (map.count("TRADE_ID")) t.id = map.at("TRADE_ID");
                        if (map.count("LAST_PRICE")) t.price = std::stod(map.at("LAST_PRICE"));
                        else if (map.count("PRICE")) t.price = std::stod(map.at("PRICE"));
                        if (map.count("LAST_SIZE")) t.size = std::stod(map.at("LAST_SIZE"));
                        else if (map.count("SIZE")) t.size = std::stod(map.at("SIZE"));
                        if (map.count("IS_BUYER_MAKER")) t.side = (map.at("IS_BUYER_MAKER") == "1" || map.at("IS_BUYER_MAKER") == "true") ? "sell" : "buy";
                        if (map.count("TIMESTAMP")) t.timestamp = map.at("TIMESTAMP");
                        else t.timestamp = message.getTimeISO();
                        trades.push_back(t);
                    }
                }
            }
        }
        return trades;
    }

    std::vector<OHLCV> fetchOHLCV(const std::string& symbol, const std::string& timeframe = "60", int limit = 100) override {
        if (shouldUseGenericOHLCV()) return fetchOHLCVGeneric(symbol, timeframe, limit);

        std::vector<OHLCV> candles;
        ccapi::Request request(ccapi::Request::Operation::GET_RECENT_CANDLESTICKS, exchangeName_, symbol);
        request.appendParam({{"CANDLESTICK_INTERVAL_SECONDS", timeframe}, {"LIMIT", std::to_string(limit)}});
        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) continue;
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        OHLCV c;
                        c.symbol = symbol;
                        if (map.count("START_TIME")) c.timestamp = map.at("START_TIME");
                        if (map.count("OPEN_PRICE")) c.open = std::stod(map.at("OPEN_PRICE"));
                        if (map.count("HIGH_PRICE")) c.high = std::stod(map.at("HIGH_PRICE"));
                        if (map.count("LOW_PRICE")) c.low = std::stod(map.at("LOW_PRICE"));
                        if (map.count("CLOSE_PRICE")) c.close = std::stod(map.at("CLOSE_PRICE"));
                        if (map.count("VOLUME")) c.volume = std::stod(map.at("VOLUME"));
                        candles.push_back(c);
                    }
                }
            }
        }
        return candles;
    }

    std::vector<Instrument> fetchInstruments() override {
        if (shouldUseGenericInstruments()) return fetchInstrumentsGeneric();

        std::vector<Instrument> instruments;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENTS, exchangeName_);
        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) continue;
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        Instrument i;
                        if (map.count("SYMBOL")) i.symbol = map.at("SYMBOL");
                        if (map.count("BASE_ASSET")) i.baseAsset = map.at("BASE_ASSET");
                        if (map.count("QUOTE_ASSET")) i.quoteAsset = map.at("QUOTE_ASSET");
                        instruments.push_back(i);
                    }
                }
            }
        }
        return instruments;
    }

    Instrument fetchInstrument(const std::string& symbol) override {
        if (shouldUseGenericInstrument()) return fetchInstrumentGeneric(symbol);

        Instrument instrument; instrument.symbol = symbol;
        ccapi::Request request(ccapi::Request::Operation::GET_INSTRUMENT, exchangeName_, symbol);
        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) continue;
                    for (const auto& element : message.getElementList()) {
                         const auto& map = element.getNameValueMap();
                         if (map.count("BASE_ASSET")) instrument.baseAsset = map.at("BASE_ASSET");
                         if (map.count("QUOTE_ASSET")) instrument.quoteAsset = map.at("QUOTE_ASSET");
                    }
                }
            }
        }
        return instrument;
    }

    std::vector<OHLCV> fetchOHLCVHistorical(const std::string& symbol, const std::string& timeframe, const std::string& startTime, const std::string& endTime, int limit) override {
        if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             std::vector<OHLCV> candles;
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Hist OHLCV Generic");
             std::string interval = "1m";
             if (timeframe == "60") interval = "1m";
             else if (timeframe == "3600") interval = "1h";
             else if (timeframe == "86400") interval = "1d";

             std::string qs = "symbol=" + symbol + "&interval=" + interval + "&limit=" + std::to_string(limit);
             qs += "&startTime=" + isoToMs(startTime) + "&endTime=" + isoToMs(endTime);

             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/klines"}, {"HTTP_QUERY_STRING", qs}});

             auto events = sendRequestSync(request);
             for(const auto& event : events) {
                 if (event.getType() == ccapi::Event::Type::RESPONSE) {
                     for(const auto& msg : event.getMessageList()) {
                         for(const auto& elem : msg.getElementList()) {
                             if(elem.getNameValueMap().count("HTTP_BODY")) {
                                 rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                                 if(!d.HasParseError() && d.IsArray()) {
                                     for(const auto& k : d.GetArray()) {
                                         OHLCV c;
                                         c.symbol = symbol;
                                         c.timestamp = msToIso(k[0].GetInt64());
                                         c.open = std::stod(k[1].GetString());
                                         c.high = std::stod(k[2].GetString());
                                         c.low = std::stod(k[3].GetString());
                                         c.close = std::stod(k[4].GetString());
                                         c.volume = std::stod(k[5].GetString());
                                         candles.push_back(c);
                                     }
                                 }
                             }
                         }
                     }
                 }
             }
             return candles;
        }

        std::vector<OHLCV> candles;
        ccapi::Request request(ccapi::Request::Operation::GET_HISTORICAL_CANDLESTICKS, exchangeName_, symbol);
        request.appendParam({
            {"CANDLESTICK_INTERVAL_SECONDS", timeframe},
            {"START_TIME", startTime},
            {"END_TIME", endTime},
            {"LIMIT", std::to_string(limit)}
        });

        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) continue;
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        OHLCV c;
                        c.symbol = symbol;
                        if (map.count("START_TIME")) c.timestamp = map.at("START_TIME");
                        if (map.count("OPEN_PRICE")) c.open = std::stod(map.at("OPEN_PRICE"));
                        if (map.count("HIGH_PRICE")) c.high = std::stod(map.at("HIGH_PRICE"));
                        if (map.count("LOW_PRICE")) c.low = std::stod(map.at("LOW_PRICE"));
                        if (map.count("CLOSE_PRICE")) c.close = std::stod(map.at("CLOSE_PRICE"));
                        if (map.count("VOLUME")) c.volume = std::stod(map.at("VOLUME"));
                        candles.push_back(c);
                    }
                }
            }
        }
        return candles;
    }

    std::vector<Trade> fetchTradesHistorical(const std::string& symbol, const std::string& startTime, const std::string& endTime, int limit) override {
        if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             std::vector<Trade> trades;
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Hist Trades Generic");
             std::string qs = "symbol=" + symbol + "&limit=" + std::to_string(limit);
             qs += "&startTime=" + isoToMs(startTime) + "&endTime=" + isoToMs(endTime);

             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/aggTrades"}, {"HTTP_QUERY_STRING", qs}});

             auto events = sendRequestSync(request);
             for(const auto& event : events) {
                 if (event.getType() == ccapi::Event::Type::RESPONSE) {
                     for(const auto& msg : event.getMessageList()) {
                         for(const auto& elem : msg.getElementList()) {
                             if(elem.getNameValueMap().count("HTTP_BODY")) {
                                 rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                                 if(!d.HasParseError() && d.IsArray()) {
                                     for(const auto& t_json : d.GetArray()) {
                                         Trade t;
                                         t.symbol = symbol;
                                         t.id = std::to_string(t_json["a"].GetInt64());
                                         t.price = std::stod(t_json["p"].GetString());
                                         t.size = std::stod(t_json["q"].GetString());
                                         t.timestamp = msToIso(t_json["T"].GetInt64());
                                         t.isBuyerMaker = t_json["m"].GetBool();
                                         t.side = t.isBuyerMaker ? "sell" : "buy";
                                         trades.push_back(t);
                                     }
                                 }
                             }
                         }
                     }
                 }
             }
             return trades;
        }

        std::vector<Trade> trades;
        ccapi::Request request(ccapi::Request::Operation::GET_HISTORICAL_TRADES, exchangeName_, symbol);
         request.appendParam({
            {"START_TIME", startTime},
            {"END_TIME", endTime},
            {"LIMIT", std::to_string(limit)}
        });
        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) continue;
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        Trade t; t.symbol = symbol;
                        if (map.count("TRADE_ID")) t.id = map.at("TRADE_ID");
                        if (map.count("LAST_PRICE")) t.price = std::stod(map.at("LAST_PRICE"));
                        else if (map.count("PRICE")) t.price = std::stod(map.at("PRICE"));
                        if (map.count("LAST_SIZE")) t.size = std::stod(map.at("LAST_SIZE"));
                        else if (map.count("SIZE")) t.size = std::stod(map.at("SIZE"));
                        if (map.count("IS_BUYER_MAKER")) t.side = (map.at("IS_BUYER_MAKER") == "1" || map.at("IS_BUYER_MAKER") == "true") ? "sell" : "buy";
                        if (map.count("TIMESTAMP")) t.timestamp = map.at("TIMESTAMP");
                        else t.timestamp = message.getTimeISO();
                        trades.push_back(t);
                    }
                }
            }
        }
        return trades;
    }

    std::string sendCustomRequest(const std::string& method, const std::string& path, const std::map<std::string, std::string>& params) override {
         ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Custom Request");
         request.appendParam({{"HTTP_METHOD", method}, {"HTTP_PATH", path}});

         std::string queryString;
         for (const auto& [k, v] : params) {
             if (!queryString.empty()) queryString += "&";
             queryString += k + "=" + v;
         }
         if (!queryString.empty()) {
             request.appendParam({{"HTTP_QUERY_STRING", queryString}});
         }

         auto events = sendRequestSync(request);
         for(const auto& event : events) {
             if (event.getType() == ccapi::Event::Type::RESPONSE) {
                 for(const auto& msg : event.getMessageList()) {
                      for(const auto& elem : msg.getElementList()) {
                          if (elem.getNameValueMap().count("HTTP_BODY")) {
                              return elem.getNameValueMap().at("HTTP_BODY");
                          }
                      }
                 }
             }
         }
         return "";
    }

    // New Public
    TickerStats fetchTicker24h(const std::string& symbol) override {
        TickerStats stats; stats.symbol = symbol;
        if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get 24h Ticker");
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/ticker/24hr"}, {"HTTP_QUERY_STRING", "symbol=" + symbol}});

             auto events = sendRequestSync(request);
             for(const auto& event : events) {
                 if (event.getType() == ccapi::Event::Type::RESPONSE) {
                     for(const auto& msg : event.getMessageList()) {
                         for(const auto& elem : msg.getElementList()) {
                             if(elem.getNameValueMap().count("HTTP_BODY")) {
                                 rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                                 if(!d.HasParseError()) {
                                     stats.priceChange = std::stod(d["priceChange"].GetString());
                                     stats.priceChangePercent = std::stod(d["priceChangePercent"].GetString());
                                     stats.weightedAvgPrice = std::stod(d["weightedAvgPrice"].GetString());
                                     stats.prevClosePrice = std::stod(d["prevClosePrice"].GetString());
                                     stats.lastPrice = std::stod(d["lastPrice"].GetString());
                                     stats.bidPrice = std::stod(d["bidPrice"].GetString());
                                     stats.askPrice = std::stod(d["askPrice"].GetString());
                                     stats.openPrice = std::stod(d["openPrice"].GetString());
                                     stats.highPrice = std::stod(d["highPrice"].GetString());
                                     stats.lowPrice = std::stod(d["lowPrice"].GetString());
                                     stats.volume = std::stod(d["volume"].GetString());
                                     stats.quoteVolume = std::stod(d["quoteVolume"].GetString());
                                     stats.openTime = d["openTime"].GetInt64();
                                     stats.closeTime = d["closeTime"].GetInt64();
                                     stats.tradeCount = d["count"].GetInt64();
                                 }
                             }
                         }
                     }
                 }
             }
        }
        return stats;
    }

    long long fetchServerTime() override {
        if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PUBLIC_REQUEST, exchangeName_, "", "Get Server Time");
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/time"}});

             auto events = sendRequestSync(request);
             for(const auto& event : events) {
                 if (event.getType() == ccapi::Event::Type::RESPONSE) {
                     for(const auto& msg : event.getMessageList()) {
                         for(const auto& elem : msg.getElementList()) {
                             if(elem.getNameValueMap().count("HTTP_BODY")) {
                                 rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                                 if(!d.HasParseError() && d.HasMember("serverTime")) {
                                     return d["serverTime"].GetInt64();
                                 }
                             }
                         }
                     }
                 }
             }
        }
        return 0;
    }

    // Private REST
    std::string createOrder(const std::string& symbol, const std::string& side, double amount, double price = 0.0) override {
        ccapi::Request request(ccapi::Request::Operation::CREATE_ORDER, exchangeName_, symbol);
        std::string sideUpper = side;
        std::transform(sideUpper.begin(), sideUpper.end(), sideUpper.begin(), ::toupper);
        request.appendParam({{"SIDE", sideUpper}, {"QUANTITY", std::to_string(amount)}});
        if (price > 0.0) request.appendParam({{"LIMIT_PRICE", std::to_string(price)}, {"ORDER_TYPE", "LIMIT"}});
        else request.appendParam({{"ORDER_TYPE", "MARKET"}});

        if (!config_.apiKey.empty()) {
            std::map<std::string, std::string> creds;
            creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
            creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
            if (!config_.passphrase.empty()) creds[ccapi::toString(exchangeName_) + "_API_PASSPHRASE"] = config_.passphrase;
            request.setCredential(creds);
        } else throw std::runtime_error("API Key required");

        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) throw std::runtime_error(message.toString());
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

    std::string cancelOrder(const std::string& symbol, const std::string& orderId) override {
        ccapi::Request request(ccapi::Request::Operation::CANCEL_ORDER, exchangeName_, symbol);
        request.appendParam({{"ORDER_ID", orderId}});

        if (!config_.apiKey.empty()) {
            std::map<std::string, std::string> creds;
            creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
            creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
            if (!config_.passphrase.empty()) creds[ccapi::toString(exchangeName_) + "_API_PASSPHRASE"] = config_.passphrase;
            request.setCredential(creds);
        } else throw std::runtime_error("API Key required");

        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) throw std::runtime_error(message.toString());
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        if (map.count("ORDER_ID")) return map.at("ORDER_ID");
                    }
                }
            }
        }
        return orderId;
    }

    std::vector<std::string> cancelAllOrders(const std::string& symbol) override {
        std::vector<std::string> cancelledIds;
        if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PRIVATE_REQUEST, exchangeName_, "", "Cancel All Orders");
             request.appendParam({{"HTTP_METHOD", "DELETE"}, {"HTTP_PATH", "/api/v3/openOrders"}, {"HTTP_QUERY_STRING", "symbol=" + symbol}});

             if (!config_.apiKey.empty()) {
                std::map<std::string, std::string> creds;
                creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
                creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
                request.setCredential(creds);
             } else throw std::runtime_error("API Key required");

             auto events = sendRequestSync(request);
             for(const auto& event : events) {
                 if (event.getType() == ccapi::Event::Type::RESPONSE) {
                     for(const auto& msg : event.getMessageList()) {
                         for(const auto& elem : msg.getElementList()) {
                             if(elem.getNameValueMap().count("HTTP_BODY")) {
                                 rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                                 if(!d.HasParseError() && d.IsArray()) {
                                     for(const auto& o : d.GetArray()) {
                                         if(o.HasMember("orderId")) cancelledIds.push_back(std::to_string(o["orderId"].GetInt64()));
                                     }
                                 }
                             }
                         }
                     }
                 }
             }
        }
        return cancelledIds;
    }

    Order fetchOrder(const std::string& symbol, const std::string& orderId) override {
        Order order; order.symbol = symbol; order.id = orderId;
        ccapi::Request request(ccapi::Request::Operation::GET_ORDER, exchangeName_, symbol);
        request.appendParam({{"ORDER_ID", orderId}});

        if (!config_.apiKey.empty()) {
            std::map<std::string, std::string> creds;
            creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
            creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
            if (!config_.passphrase.empty()) creds[ccapi::toString(exchangeName_) + "_API_PASSPHRASE"] = config_.passphrase;
            request.setCredential(creds);
        } else throw std::runtime_error("API Key required");

        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) throw std::runtime_error(message.toString());
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        if (map.count("ORDER_ID")) order.id = map.at("ORDER_ID");
                        if (map.count("SIDE")) order.side = map.at("SIDE");
                        if (map.count("LIMIT_PRICE")) order.price = std::stod(map.at("LIMIT_PRICE"));
                        if (map.count("QUANTITY")) order.size = std::stod(map.at("QUANTITY"));
                        if (map.count("CUMULATIVE_FILLED_QUANTITY")) order.filled = std::stod(map.at("CUMULATIVE_FILLED_QUANTITY"));
                        if (map.count("STATUS")) order.status = map.at("STATUS");
                    }
                }
            }
        }
        return order;
    }

    std::vector<Order> fetchOpenOrders(const std::string& symbol) override {
        std::vector<Order> orders;
        ccapi::Request request(ccapi::Request::Operation::GET_OPEN_ORDERS, exchangeName_, symbol);

        if (!config_.apiKey.empty()) {
            std::map<std::string, std::string> creds;
            creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
            creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
            if (!config_.passphrase.empty()) creds[ccapi::toString(exchangeName_) + "_API_PASSPHRASE"] = config_.passphrase;
            request.setCredential(creds);
        } else throw std::runtime_error("API Key required");

        auto events = sendRequestSync(request);
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) throw std::runtime_error(message.toString());
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        Order order; order.symbol = symbol;
                        if (map.count("ORDER_ID")) order.id = map.at("ORDER_ID");
                        if (map.count("SIDE")) order.side = map.at("SIDE");
                        if (map.count("LIMIT_PRICE")) order.price = std::stod(map.at("LIMIT_PRICE"));
                        if (map.count("QUANTITY")) order.size = std::stod(map.at("QUANTITY"));
                        if (map.count("CUMULATIVE_FILLED_QUANTITY")) order.filled = std::stod(map.at("CUMULATIVE_FILLED_QUANTITY"));
                        if (map.count("STATUS")) order.status = map.at("STATUS");
                        orders.push_back(order);
                    }
                }
            }
        }
        return orders;
    }

    std::vector<Trade> fetchMyTrades(const std::string& symbol, int limit = 100) override {
        if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             std::vector<Trade> trades;
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PRIVATE_REQUEST, exchangeName_, "", "Get My Trades");
             std::string qs = "symbol=" + symbol + "&limit=" + std::to_string(limit);
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/myTrades"}, {"HTTP_QUERY_STRING", qs}});

             if (!config_.apiKey.empty()) {
                std::map<std::string, std::string> creds;
                creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
                creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
                request.setCredential(creds);
             } else throw std::runtime_error("API Key required");

             auto events = sendRequestSync(request);
             for(const auto& event : events) {
                 if (event.getType() == ccapi::Event::Type::RESPONSE) {
                     for(const auto& msg : event.getMessageList()) {
                         for(const auto& elem : msg.getElementList()) {
                             if(elem.getNameValueMap().count("HTTP_BODY")) {
                                 rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                                 if(!d.HasParseError() && d.IsArray()) {
                                     for(const auto& t_json : d.GetArray()) {
                                         Trade t;
                                         t.symbol = symbol;
                                         if(t_json.HasMember("id")) t.id = std::to_string(t_json["id"].GetInt64());
                                         if(t_json.HasMember("price")) t.price = std::stod(t_json["price"].GetString());
                                         if(t_json.HasMember("qty")) t.size = std::stod(t_json["qty"].GetString());
                                         if(t_json.HasMember("time")) t.timestamp = msToIso(t_json["time"].GetInt64());
                                         if(t_json.HasMember("isBuyer")) t.side = t_json["isBuyer"].GetBool() ? "buy" : "sell";
                                         trades.push_back(t);
                                     }
                                 }
                             }
                         }
                     }
                 }
             }
             return trades;
        }
        throw std::runtime_error("Not implemented for this exchange");
    }

    std::map<std::string, double> fetchBalance() override {
        ccapi::Request request(ccapi::Request::Operation::GET_ACCOUNTS, exchangeName_);
        if (!config_.apiKey.empty()) {
             std::map<std::string, std::string> creds;
            creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
            creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
            if (!config_.passphrase.empty()) creds[ccapi::toString(exchangeName_) + "_API_PASSPHRASE"] = config_.passphrase;
            request.setCredential(creds);
        } else throw std::runtime_error("API Key required");

        auto events = sendRequestSync(request);
        std::map<std::string, double> balances;
        for (const auto& event : events) {
            if (event.getType() == ccapi::Event::Type::RESPONSE) {
                for (const auto& message : event.getMessageList()) {
                     if (message.getType() == ccapi::Message::Type::RESPONSE_ERROR) throw std::runtime_error(message.toString());
                    for (const auto& element : message.getElementList()) {
                        const auto& map = element.getNameValueMap();
                        std::string asset;
                        double free = 0.0;
                        if (map.count("ASSET")) asset = map.at("ASSET");
                        else if (map.count("CURRENCY")) asset = map.at("CURRENCY");
                        if (map.count("FREE")) free = std::stod(map.at("FREE"));
                        else if (map.count("AVAILABLE")) free = std::stod(map.at("AVAILABLE"));
                        if (!asset.empty()) balances[asset] = free;
                    }
                }
            }
        }
        return balances;
    }

    TradingFees fetchTradingFees(const std::string& symbol) override {
        TradingFees fees{0.0, 0.0};
        if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             // For Binance, try Account Info for commission rates (generic global)
             // or Generic Private Request to /sapi/v1/asset/tradeFee
             if (config_.apiKey.empty()) throw std::runtime_error("API Key required");

             // Fallback to Account Info commission which is in basis points usually?
             AccountInfo info = fetchAccountInfo();
             // Binance commissions are maker/taker basis points * 100? or raw?
             // Usually it returns makerCommission: 10 (meaning 0.10% or 10 bps).
             fees.maker = info.makerCommission / 10000.0;
             fees.taker = info.takerCommission / 10000.0;
        }
        return fees;
    }

    AccountInfo fetchAccountInfo() override {
        AccountInfo info;
        if (exchangeName_ == "binance-us" || exchangeName_ == "binance") {
             ccapi::Request request(ccapi::Request::Operation::GENERIC_PRIVATE_REQUEST, exchangeName_, "", "Get Account Info");
             request.appendParam({{"HTTP_METHOD", "GET"}, {"HTTP_PATH", "/api/v3/account"}});

             if (!config_.apiKey.empty()) {
                std::map<std::string, std::string> creds;
                creds[ccapi::toString(exchangeName_) + "_API_KEY"] = config_.apiKey;
                creds[ccapi::toString(exchangeName_) + "_API_SECRET"] = config_.apiSecret;
                request.setCredential(creds);
             } else throw std::runtime_error("API Key required");

             auto events = sendRequestSync(request);
             for(const auto& event : events) {
                 if (event.getType() == ccapi::Event::Type::RESPONSE) {
                     for(const auto& msg : event.getMessageList()) {
                         for(const auto& elem : msg.getElementList()) {
                             if(elem.getNameValueMap().count("HTTP_BODY")) {
                                 rapidjson::Document d; d.Parse(elem.getNameValueMap().at("HTTP_BODY").c_str());
                                 if(!d.HasParseError()) {
                                     if(d.HasMember("makerCommission")) info.makerCommission = d["makerCommission"].GetInt();
                                     if(d.HasMember("takerCommission")) info.takerCommission = d["takerCommission"].GetInt();
                                     if(d.HasMember("canTrade")) info.canTrade = d["canTrade"].GetBool();
                                     if(d.HasMember("canWithdraw")) info.canWithdraw = d["canWithdraw"].GetBool();
                                     if(d.HasMember("canDeposit")) info.canDeposit = d["canDeposit"].GetBool();
                                     if(d.HasMember("updateTime")) info.updateTime = d["updateTime"].GetInt64();
                                     if(d.HasMember("accountType")) info.accountType = d["accountType"].GetString();
                                     if(d.HasMember("balances") && d["balances"].IsArray()) {
                                         for(const auto& b : d["balances"].GetArray()) {
                                             std::string asset = b["asset"].GetString();
                                             double free = std::stod(b["free"].GetString());
                                             info.balances[asset] = free;
                                         }
                                     }
                                 }
                             }
                         }
                     }
                 }
             }
        }
        return info;
    }

protected:
    class GenericEventHandler : public ccapi::EventHandler {
        GenericExchange* parent_;
    public:
        GenericEventHandler(GenericExchange* parent) : parent_(parent) {}
        void processEvent(const ccapi::Event& event, ccapi::Session* session) override {
            if (event.getType() == ccapi::Event::Type::SUBSCRIPTION_DATA) {
                for (const auto& message : event.getMessageList()) {
                    if (message.getType() == ccapi::Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH) {
                        if (parent_->onOrderBook_) {
                             OrderBook ob;
                             ob.timestamp = message.getTimeISO();
                             for (const auto& element : message.getElementList()) {
                                 const auto& map = element.getNameValueMap();
                                 if (map.count("BID_PRICE")) ob.bids.push_back(OrderBookEntry{std::stod(map.at("BID_PRICE")), std::stod(map.at("BID_SIZE"))});
                                 if (map.count("ASK_PRICE")) ob.asks.push_back(OrderBookEntry{std::stod(map.at("ASK_PRICE")), std::stod(map.at("ASK_SIZE"))});
                             }
                             parent_->onOrderBook_(ob);
                        }
                        if (parent_->onTicker_) {
                             Ticker t;
                             t.timestamp = message.getTimeISO();
                             for (const auto& element : message.getElementList()) {
                                 const auto& map = element.getNameValueMap();
                                 if (map.count("BID_PRICE")) t.bidPrice = std::stod(map.at("BID_PRICE"));
                                 if (map.count("ASK_PRICE")) t.askPrice = std::stod(map.at("ASK_PRICE"));
                             }
                             if (t.bidPrice > 0) parent_->onTicker_(t);
                        }
                    }
                    else if (message.getType() == ccapi::Message::Type::MARKET_DATA_EVENTS_TRADE) {
                        if (parent_->onTrade_) {
                            for (const auto& element : message.getElementList()) {
                                const auto& map = element.getNameValueMap();
                                Trade t;
                                t.timestamp = message.getTimeISO();
                                if (map.count("LAST_PRICE")) t.price = std::stod(map.at("LAST_PRICE"));
                                if (map.count("LAST_SIZE")) t.size = std::stod(map.at("LAST_SIZE"));
                                if (map.count("IS_BUYER_MAKER")) {
                                    bool isBuyerMaker = (map.at("IS_BUYER_MAKER") == "1" || map.at("IS_BUYER_MAKER") == "true");
                                    t.side = isBuyerMaker ? "sell" : "buy";
                                }
                                parent_->onTrade_(t);
                            }
                        }
                    }
                    else if (message.getType() == ccapi::Message::Type::MARKET_DATA_EVENTS_CANDLESTICK) {
                         if (parent_->onOHLCV_) {
                             for (const auto& element : message.getElementList()) {
                                 const auto& map = element.getNameValueMap();
                                 OHLCV c;
                                 if (map.count("INSTRUMENT")) c.symbol = map.at("INSTRUMENT");
                                 if (map.count("OPEN_PRICE")) c.open = std::stod(map.at("OPEN_PRICE"));
                                 if (map.count("CLOSE_PRICE")) c.close = std::stod(map.at("CLOSE_PRICE"));
                                 if (map.count("HIGH_PRICE")) c.high = std::stod(map.at("HIGH_PRICE"));
                                 if (map.count("LOW_PRICE")) c.low = std::stod(map.at("LOW_PRICE"));
                                 if (map.count("VOLUME")) c.volume = std::stod(map.at("VOLUME"));
                                 if (map.count("TIMESTAMP")) c.timestamp = map.at("TIMESTAMP"); // Ensure timestamp is set
                                 else c.timestamp = message.getTimeISO();

                                 parent_->onOHLCV_(c);
                             }
                         }
                    }
                    else if (message.getType() == ccapi::Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE) {
                        if (parent_->onOrderUpdate_) {
                            for (const auto& element : message.getElementList()) {
                                const auto& map = element.getNameValueMap();
                                Order o;
                                if (map.count("ORDER_ID")) o.id = map.at("ORDER_ID");
                                if (map.count("CLIENT_ORDER_ID")) o.clientOrderId = map.at("CLIENT_ORDER_ID");
                                if (map.count("SIDE")) o.side = map.at("SIDE");
                                if (map.count("LIMIT_PRICE")) o.price = std::stod(map.at("LIMIT_PRICE"));
                                if (map.count("QUANTITY")) o.size = std::stod(map.at("QUANTITY"));
                                if (map.count("CUMULATIVE_FILLED_QUANTITY")) o.filled = std::stod(map.at("CUMULATIVE_FILLED_QUANTITY"));
                                if (map.count("STATUS")) o.status = map.at("STATUS");
                                o.timestamp = message.getTimeISO();
                                parent_->onOrderUpdate_(o);
                            }
                        }
                    }
                    else if (message.getType() == ccapi::Message::Type::EXECUTION_MANAGEMENT_EVENTS_BALANCE_UPDATE) {
                        if (parent_->onAccountUpdate_) {
                            for (const auto& element : message.getElementList()) {
                                const auto& map = element.getNameValueMap();
                                BalanceUpdate b;
                                if (map.count("ASSET")) b.asset = map.at("ASSET");
                                if (map.count("QUANTITY_AVAILABLE_FOR_TRADING")) b.free = std::stod(map.at("QUANTITY_AVAILABLE_FOR_TRADING"));
                                b.timestamp = message.getTimeISO();
                                parent_->onAccountUpdate_(b);
                            }
                        }
                    }
                }
            }
        }
    };
};

}
