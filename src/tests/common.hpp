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
            // Try to proceed anyway if possible with a hardcoded symbol?
            // But usually this means API connection or mapping failed.
            // We'll return to let the dev fix it.
            return;
        }

        // Pick a pair (e.g. BTC/USDT or similar)
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

        // 6. WS
        std::cout << "[Step 6] WS Subscriptions (10s)..." << std::endl;
        std::atomic<int> updates{0};
        exchange.setOnTicker([&](const Ticker&){ updates++; });
        exchange.setOnOrderBook([&](const OrderBook&){ updates++; });

        exchange.subscribeTicker(symbol);
        exchange.subscribeOrderBook(symbol);

        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << "  WS Updates received: " << updates << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
    }
}
