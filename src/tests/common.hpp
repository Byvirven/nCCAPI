#pragma once

#include "../UnifiedExchange.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

using namespace unified_crypto;

inline void run_exchange_test(const std::string& exchange_name) {
    std::cout << "------------------------------------------------------------" << std::endl;
    std::cout << "TESTING EXCHANGE: " << exchange_name << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    try {
        UnifiedExchange exchange(exchange_name);

        // 1. Instruments
        std::cout << "[Step 1] Fetching Instruments..." << std::endl;
        auto instruments = exchange.fetchInstruments();
        std::cout << "  Found " << instruments.size() << " instruments." << std::endl;

        if (instruments.empty()) {
            std::cerr << "  CRITICAL FAILURE: No instruments found." << std::endl;
            return;
        }

        std::string symbol = instruments[0].symbol;
        for(const auto& i : instruments) {
            if(i.baseAsset == "BTC" && (i.quoteAsset == "USDT" || i.quoteAsset == "USD")) {
                symbol = i.symbol;
                break;
            }
        }
        std::cout << "  Selected Symbol: " << symbol << std::endl;

        // 2. REST Ticker
        std::cout << "[Step 2] REST Ticker..." << std::endl;
        try {
            Ticker t = exchange.fetchTicker(symbol);
            std::cout << "  Ticker: " << t.lastPrice << " (Bid: " << t.bidPrice << ")" << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 3. REST Book
        std::cout << "[Step 3] REST OrderBook..." << std::endl;
        try {
            OrderBook b = exchange.fetchOrderBook(symbol, 5);
            std::cout << "  Book: " << b.bids.size() << " bids" << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 4. REST Trades
        std::cout << "[Step 4] REST Trades..." << std::endl;
        try {
            auto trades = exchange.fetchTrades(symbol, 5);
            std::cout << "  Trades: " << trades.size() << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 5. REST OHLCV
        std::cout << "[Step 5] REST OHLCV..." << std::endl;
        try {
            auto candles = exchange.fetchOHLCV(symbol, "60", 5);
            std::cout << "  OHLCV: " << candles.size() << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 7. Single Instrument
        std::cout << "[Step 7] Single Instrument..." << std::endl;
        try {
            Instrument i = exchange.fetchInstrument(symbol);
            std::cout << "  Status: " << i.status << " | Tick: " << i.tickSize << " | Step: " << i.stepSize << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 8. Historical OHLCV
        std::cout << "[Step 8] Historical OHLCV..." << std::endl;
        try {
            std::string start = "2024-01-01T00:00:00Z";
            std::string end = "2024-01-01T04:00:00Z";
            auto candles = exchange.fetchOHLCVHistorical(symbol, "3600", start, end, 10);
            std::cout << "  Hist Candles: " << candles.size() << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 9. Historical Trades
        std::cout << "[Step 9] Historical Trades..." << std::endl;
        try {
            std::string start = "2024-01-01T00:00:00Z";
            std::string end = "2024-01-01T00:10:00Z";
            auto trades = exchange.fetchTradesHistorical(symbol, start, end, 10);
            std::cout << "  Hist Trades: " << trades.size() << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 10. Custom Request
        std::cout << "[Step 10] Custom Request..." << std::endl;
        try {
            std::string res = exchange.sendCustomRequest("GET", "/api/v3/ping");
            std::cout << "  Ping: " << (res.empty() ? "{}" : res) << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 11. Private REST Connectivity (Expected Fail)
        std::cout << "[Step 11] Private REST Check..." << std::endl;
        try {
            exchange.fetchMyTrades(symbol, 5);
            std::cerr << "  ERROR: Should have failed (no key)." << std::endl;
        } catch(const std::exception& e) {
            std::cout << "  Expected Failure: " << e.what() << std::endl;
        }

        // 6 & 12. WS Subscriptions
        std::cout << "[Step 12] WS Subscriptions (Public + Private)..." << std::endl;
        std::atomic<int> updates{0};
        exchange.setOnTicker([&](const Ticker&){ updates++; });
        exchange.setOnOrderBook([&](const OrderBook&){ updates++; });
        exchange.setOnOrderUpdate([&](const Order&){ std::cout << "  Order Update!" << std::endl; });
        exchange.setOnAccountUpdate([&](const BalanceUpdate&){ std::cout << "  Account Update!" << std::endl; });

        exchange.subscribeTicker(symbol);
        exchange.subscribeOrderBook(symbol);

        // Private Subscriptions (Might print error logs internally)
        try {
            exchange.subscribeOrderUpdates(symbol);
            exchange.subscribeAccountUpdates();
        } catch(...) {}

        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << "  WS Updates received: " << updates << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
    }
}
