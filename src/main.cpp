#include "UnifiedExchange.hpp"
#include <iostream>

namespace ccapi {
    Logger* Logger::logger = nullptr;
}

using namespace unified_crypto;

int main() {
    try {
        // Initialize Logger (Optional, for debugging CCAPI internals)
        ccapi::Logger* logger = new ccapi::Logger();
        ccapi::Logger::logger = logger;

        try {
            std::cout << "--- Testing BinanceUS Public API ---" << std::endl;
            // Use binance-us to avoid geo-blocking. CCAPI expects "binance-us"
            UnifiedExchange binance("binance-us");

            std::cout << "Fetching Ticker for BTCUSDT..." << std::endl;
            Ticker ticker = binance.fetchTicker("BTCUSDT");
            std::cout << "Ticker: Bid=" << ticker.bidPrice << " Ask=" << ticker.askPrice << std::endl;

            std::cout << "Fetching OrderBook for BTCUSDT..." << std::endl;
            OrderBook book = binance.fetchOrderBook("BTCUSDT", 5);
            std::cout << "OrderBook Bids: " << book.bids.size() << std::endl;

            std::cout << "Fetching OHLCV for BTCUSDT (1 min)..." << std::endl;
            auto candles = binance.fetchOHLCV("BTCUSDT", "60", 5);
            std::cout << "Candles fetched: " << candles.size() << std::endl;
            if(!candles.empty()) {
                const auto& c = candles[0];
                std::cout << " First Candle: " << c.timestamp << " O:" << c.open << " C:" << c.close << std::endl;
            }

            // Private API Example (Commented out as no valid keys are provided)
            /*
            ExchangeConfig config;
            config.apiKey = "YOUR_API_KEY";
            config.apiSecret = "YOUR_API_SECRET";
            UnifiedExchange binancePrivate("binance-us", config);

            auto balances = binancePrivate.fetchBalance();
            std::cout << "USDT Balance: " << balances["USDT"] << std::endl;

            std::string orderId = binancePrivate.createOrder("BTCUSDT", "BUY", 0.0001, 20000.0);
            std::cout << "Order Created: " << orderId << std::endl;
            */

        } catch (const std::exception& e) {
            std::cerr << "BinanceUS Test Failed: " << e.what() << std::endl;
        }

        std::cout << "\n--- Testing Coinbase Public API ---" << std::endl;
        UnifiedExchange coinbase("coinbase");

        std::cout << "Fetching Ticker for BTC-USD..." << std::endl;
        Ticker cbTicker = coinbase.fetchTicker("BTC-USD");
        std::cout << "Ticker: Bid=" << cbTicker.bidPrice << " Ask=" << cbTicker.askPrice << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
