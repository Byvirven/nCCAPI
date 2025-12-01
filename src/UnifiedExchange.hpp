#pragma once

#include "Exchange.hpp"
#include "exchanges/GenericExchange.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace unified_crypto {

class UnifiedExchange {
public:
    UnifiedExchange(const std::string& exchange, const ExchangeConfig& config = {}) {
        // Factory Logic
        // Future: if (exchange == "binance") impl_ = std::make_unique<BinanceExchange>(exchange, config);
        // Else generic
        impl_ = std::make_unique<GenericExchange>(exchange, config);
    }

    // Proxy Methods
    void setOnTicker(Exchange::TickerCallback cb) { impl_->setOnTicker(cb); }
    void setOnOrderBook(Exchange::OrderBookCallback cb) { impl_->setOnOrderBook(cb); }
    void setOnTrade(Exchange::TradeCallback cb) { impl_->setOnTrade(cb); }
    void setOnOHLCV(Exchange::OHLCVCallback cb) { impl_->setOnOHLCV(cb); }

    void subscribeTicker(const std::string& symbol) { impl_->subscribeTicker(symbol); }
    void subscribeOrderBook(const std::string& symbol, int depth = 10) { impl_->subscribeOrderBook(symbol, depth); }
    void subscribeTrades(const std::string& symbol) { impl_->subscribeTrades(symbol); }
    void subscribeOHLCV(const std::string& symbol, const std::string& interval = "60") { impl_->subscribeOHLCV(symbol, interval); }

    Ticker fetchTicker(const std::string& symbol) { return impl_->fetchTicker(symbol); }
    OrderBook fetchOrderBook(const std::string& symbol, int limit = 10) { return impl_->fetchOrderBook(symbol, limit); }
    std::vector<Trade> fetchTrades(const std::string& symbol, int limit = 100) { return impl_->fetchTrades(symbol, limit); }
    std::vector<OHLCV> fetchOHLCV(const std::string& symbol, const std::string& timeframe = "60", int limit = 100) { return impl_->fetchOHLCV(symbol, timeframe, limit); }
    std::vector<Instrument> fetchInstruments() { return impl_->fetchInstruments(); }

    std::string createOrder(const std::string& symbol, const std::string& side, double amount, double price = 0.0) {
        return impl_->createOrder(symbol, side, amount, price);
    }
    std::map<std::string, double> fetchBalance() { return impl_->fetchBalance(); }

private:
    std::unique_ptr<Exchange> impl_;
};

}
