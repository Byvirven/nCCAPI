#pragma once

#include "../UnifiedExchange.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>

using namespace unified_crypto;

// Helper functions for verbose output
inline void printInstrument(const Instrument& i) {
    std::cout << "  [Instrument] " << i.symbol
              << " Base=" << i.baseAsset << " Quote=" << i.quoteAsset
              << " Status=" << i.status << " Type=" << i.type
              << " Tick=" << i.tickSize << " MinQty=" << i.minSize << " Step=" << i.stepSize << std::endl;
}

inline void printTicker(const Ticker& t) {
    std::cout << "  [Ticker] " << t.symbol << " Last=" << t.lastPrice
              << " Bid=" << t.bidPrice << " (" << t.bidSize << ") Ask=" << t.askPrice << " (" << t.askSize << ")"
              << " Time=" << t.timestamp << std::endl;
}

inline void printOrderBook(const OrderBook& ob) {
    std::cout << "  [OrderBook] " << ob.symbol << " Time=" << ob.timestamp << std::endl;
    std::cout << "    Bids (Top 5):" << std::endl;
    for(size_t i=0; i<std::min(ob.bids.size(), size_t(5)); ++i) {
        std::cout << "      " << ob.bids[i].price << " x " << ob.bids[i].size << std::endl;
    }
    std::cout << "    Asks (Top 5):" << std::endl;
    for(size_t i=0; i<std::min(ob.asks.size(), size_t(5)); ++i) {
        std::cout << "      " << ob.asks[i].price << " x " << ob.asks[i].size << std::endl;
    }
}

inline void printTrade(const Trade& t) {
    std::cout << "  [Trade] ID=" << t.id << " Price=" << t.price << " Size=" << t.size
              << " Side=" << t.side << " Maker=" << (t.isBuyerMaker ? "Yes" : "No")
              << " Time=" << t.timestamp << std::endl;
}

inline void printOHLCV(const OHLCV& c) {
    std::cout << "  [OHLCV] " << (c.symbol.empty() ? "" : c.symbol + " ") << "Time=" << c.timestamp << " O=" << c.open << " H=" << c.high
              << " L=" << c.low << " C=" << c.close << " V=" << c.volume << std::endl;
}

inline void printOrder(const Order& o) {
    std::cout << "  [Order] ID=" << o.id << " Symbol=" << o.symbol << " Side=" << o.side
              << " Price=" << o.price << " Size=" << o.size << " Filled=" << o.filled
              << " Status=" << o.status << " ClientID=" << o.clientOrderId
              << " Time=" << o.timestamp << std::endl;
}

inline void printBalanceUpdate(const BalanceUpdate& b) {
    std::cout << "  [Balance] Asset=" << b.asset << " Free=" << b.free
              << " Locked=" << b.locked << " Time=" << b.timestamp << std::endl;
}

inline void run_exchange_test(const std::string& exchange_name, bool verbose = false) {
    std::cout << "------------------------------------------------------------" << std::endl;
    std::cout << "TESTING EXCHANGE: " << exchange_name << (verbose ? " (VERBOSE)" : "") << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    try {
        UnifiedExchange exchange(exchange_name);

        // 1. Instruments
        std::cout << "[Step 1] Fetching Instruments..." << std::endl;
        auto instruments = exchange.fetchInstruments();
        std::cout << "  Found " << instruments.size() << " instruments." << std::endl;

        if (verbose) {
            for (const auto& inst : instruments) {
                printInstrument(inst);
            }
        }

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
            if (verbose) printTicker(t);
            else std::cout << "  Ticker: " << t.lastPrice << " (Bid: " << t.bidPrice << ")" << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 3. REST Book
        std::cout << "[Step 3] REST OrderBook..." << std::endl;
        try {
            OrderBook b = exchange.fetchOrderBook(symbol, 5);
            if (verbose) printOrderBook(b);
            else std::cout << "  Book: " << b.bids.size() << " bids" << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 4. REST Trades
        std::cout << "[Step 4] REST Trades..." << std::endl;
        try {
            auto trades = exchange.fetchTrades(symbol, 5);
            if (verbose) {
                for(const auto& tr : trades) printTrade(tr);
            } else {
                std::cout << "  Trades: " << trades.size() << std::endl;
            }
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 5. REST OHLCV
        std::cout << "[Step 5] REST OHLCV..." << std::endl;
        try {
            auto candles = exchange.fetchOHLCV(symbol, "60", 5);
            if (verbose) {
                for(const auto& c : candles) printOHLCV(c);
            } else {
                std::cout << "  OHLCV: " << candles.size() << std::endl;
            }
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 7. Single Instrument
        std::cout << "[Step 7] Single Instrument..." << std::endl;
        try {
            Instrument i = exchange.fetchInstrument(symbol);
            if (verbose) printInstrument(i);
            else std::cout << "  Status: " << i.status << " | Tick: " << i.tickSize << " | Step: " << i.stepSize << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 8. Historical OHLCV
        std::cout << "[Step 8] Historical OHLCV..." << std::endl;
        try {
            std::string start = "2024-01-01T00:00:00Z";
            std::string end = "2024-01-01T04:00:00Z";
            auto candles = exchange.fetchOHLCVHistorical(symbol, "3600", start, end, 10);
            if (verbose) {
                for(const auto& c : candles) printOHLCV(c);
            } else {
                std::cout << "  Hist Candles: " << candles.size() << std::endl;
            }
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 9. Historical Trades
        std::cout << "[Step 9] Historical Trades..." << std::endl;
        try {
            std::string start = "2024-01-01T00:00:00Z";
            std::string end = "2024-01-01T00:10:00Z";
            auto trades = exchange.fetchTradesHistorical(symbol, start, end, 10);
            if (verbose) {
                for(const auto& tr : trades) printTrade(tr);
            } else {
                std::cout << "  Hist Trades: " << trades.size() << std::endl;
            }
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

        // 13. Ticker 24h
        std::cout << "[Step 13] Ticker 24h..." << std::endl;
        try {
            TickerStats s = exchange.fetchTicker24h(symbol);
            if(verbose) {
                std::cout << "  [Ticker24h] " << s.symbol << " Last=" << s.lastPrice << " Vol=" << s.volume
                          << " Change%=" << s.priceChangePercent << " High=" << s.highPrice << " Low=" << s.lowPrice << std::endl;
            } else {
                std::cout << "  Ticker 24h: Last=" << s.lastPrice << " Vol=" << s.volume << std::endl;
            }
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 14. Server Time
        std::cout << "[Step 14] Server Time..." << std::endl;
        try {
            long long time = exchange.fetchServerTime();
            std::cout << "  Server Time: " << time << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "  FAILED: " << e.what() << std::endl;
        }

        // 15. Account Info (Private Check)
        std::cout << "[Step 15] Account Info (Private Check)..." << std::endl;
        try {
            exchange.fetchAccountInfo();
            std::cerr << "  ERROR: Should have failed (no key)." << std::endl;
        } catch(const std::exception& e) {
            std::cout << "  Expected Failure: " << e.what() << std::endl;
        }

        // 12. WS Subscriptions
        std::cout << "[Step 12] WS Subscriptions (Public + Private)..." << std::endl;
        std::atomic<int> updates{0};
        exchange.setOnTicker([&](const Ticker& t){
            updates++;
            if(verbose) printTicker(t);
        });
        exchange.setOnOrderBook([&](const OrderBook& ob){
            updates++;
            if(verbose) printOrderBook(ob);
        });
        exchange.setOnOHLCV([&](const OHLCV& c){
            updates++;
            if(verbose) printOHLCV(c);
        });
        exchange.setOnOrderUpdate([&](const Order& o){
            if(verbose) printOrder(o);
            else std::cout << "  [WS Order] ID=" << o.id << " Status=" << o.status << std::endl;
        });
        exchange.setOnAccountUpdate([&](const BalanceUpdate& b){
            if(verbose) printBalanceUpdate(b);
            else std::cout << "  [WS Balance] Asset=" << b.asset << " Free=" << b.free << std::endl;
        });

        // Dual Pair Test for OHLCV
        std::string symbol2 = "";
        for(const auto& i : instruments) {
            if(i.symbol != symbol && i.baseAsset == "ETH" && (i.quoteAsset == "USDT" || i.quoteAsset == "USD")) {
                symbol2 = i.symbol;
                break;
            }
        }
        if(symbol2.empty() && instruments.size() > 1) symbol2 = instruments[1].symbol;

        std::cout << "  Subscribing Ticker/Book for " << symbol << std::endl;
        exchange.subscribeTicker(symbol);
        exchange.subscribeOrderBook(symbol);

        std::cout << "  Subscribing OHLCV for " << symbol << " and " << symbol2 << std::endl;
        exchange.subscribeOHLCV(symbol);
        if(!symbol2.empty()) exchange.subscribeOHLCV(symbol2);

        try {
            exchange.subscribeOrderUpdates(symbol);
            exchange.subscribeAccountUpdates();
        } catch(...) {}

        std::this_thread::sleep_for(std::chrono::seconds(15));
        std::cout << "  WS Updates received: " << updates << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
    }
}
