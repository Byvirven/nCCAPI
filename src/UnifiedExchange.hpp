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
        impl_ = std::make_unique<GenericExchange>(exchange, config);
    }

    // Proxy Methods
    void setOnTicker(Exchange::TickerCallback cb) { impl_->setOnTicker(cb); }
    void setOnOrderBook(Exchange::OrderBookCallback cb) { impl_->setOnOrderBook(cb); }
    void setOnTrade(Exchange::TradeCallback cb) { impl_->setOnTrade(cb); }
    void setOnOHLCV(Exchange::OHLCVCallback cb) { impl_->setOnOHLCV(cb); }
    void setOnOrderUpdate(Exchange::OrderUpdateCallback cb) { impl_->setOnOrderUpdate(cb); }
    void setOnAccountUpdate(Exchange::AccountUpdateCallback cb) { impl_->setOnAccountUpdate(cb); }

    void subscribeTicker(const std::string& symbol) { impl_->subscribeTicker(symbol); }
    void subscribeOrderBook(const std::string& symbol, int depth = 10) { impl_->subscribeOrderBook(symbol, depth); }
    void subscribeTrades(const std::string& symbol) { impl_->subscribeTrades(symbol); }
    void subscribeOHLCV(const std::string& symbol, const std::string& interval = "60") { impl_->subscribeOHLCV(symbol, interval); }
    void subscribeOrderUpdates(const std::string& symbol) { impl_->subscribeOrderUpdates(symbol); }
    void subscribeAccountUpdates() { impl_->subscribeAccountUpdates(); }

    Ticker fetchTicker(const std::string& symbol) { return impl_->fetchTicker(symbol); }
    OrderBook fetchOrderBook(const std::string& symbol, int limit = 10) { return impl_->fetchOrderBook(symbol, limit); }
    std::vector<Trade> fetchTrades(const std::string& symbol, int limit = 100) { return impl_->fetchTrades(symbol, limit); }
    std::vector<OHLCV> fetchOHLCV(const std::string& symbol, const std::string& timeframe = "60", int limit = 100) { return impl_->fetchOHLCV(symbol, timeframe, limit); }
    std::vector<Instrument> fetchInstruments() { return impl_->fetchInstruments(); }

    Instrument fetchInstrument(const std::string& symbol) { return impl_->fetchInstrument(symbol); }
    std::vector<OHLCV> fetchOHLCVHistorical(const std::string& symbol, const std::string& timeframe, const std::string& startTime, const std::string& endTime, int limit = 1000) {
        return impl_->fetchOHLCVHistorical(symbol, timeframe, startTime, endTime, limit);
    }
    std::vector<Trade> fetchTradesHistorical(const std::string& symbol, const std::string& startTime, const std::string& endTime, int limit = 1000) {
        return impl_->fetchTradesHistorical(symbol, startTime, endTime, limit);
    }
    std::string sendCustomRequest(const std::string& method, const std::string& path, const std::map<std::string, std::string>& params = {}) {
        return impl_->sendCustomRequest(method, path, params);
    }

    TickerStats fetchTicker24h(const std::string& symbol) { return impl_->fetchTicker24h(symbol); }
    long long fetchServerTime() { return impl_->fetchServerTime(); }

    std::string createOrder(const std::string& symbol, const std::string& side, double amount, double price = 0.0) {
        return impl_->createOrder(symbol, side, amount, price);
    }
    std::string cancelOrder(const std::string& symbol, const std::string& orderId) {
        return impl_->cancelOrder(symbol, orderId);
    }
    Order fetchOrder(const std::string& symbol, const std::string& orderId) {
        return impl_->fetchOrder(symbol, orderId);
    }
    std::vector<Order> fetchOpenOrders(const std::string& symbol) {
        return impl_->fetchOpenOrders(symbol);
    }
    std::vector<Trade> fetchMyTrades(const std::string& symbol, int limit = 100) {
        return impl_->fetchMyTrades(symbol, limit);
    }
    std::map<std::string, double> fetchBalance() { return impl_->fetchBalance(); }
    AccountInfo fetchAccountInfo() { return impl_->fetchAccountInfo(); }

private:
    std::unique_ptr<Exchange> impl_;
};

}
