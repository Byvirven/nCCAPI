#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include "nccapi/client.hpp"

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"

void print_candle_vector(const std::vector<nccapi::Candle>& candles) {
    if (candles.empty()) {
        std::cout << YELLOW << "No candles returned." << RESET << std::endl;
        return;
    }

    std::cout << "Received " << candles.size() << " candles." << std::endl;
    std::cout << "First: " << candles.front().toString() << std::endl;
    std::cout << "Last:  " << candles.back().toString() << std::endl;
}

int main() {
    nccapi::Client client;
    std::string exchange_name = "bitmart";
    // Bitmart limit is 200. We request 500 minutes.
    int duration_minutes = 500;
    std::string symbol = "BTC_USDT";

    std::cout << CYAN << "Testing exchange: " << exchange_name << RESET << std::endl;

    try {
        // Time calc
        auto now = std::chrono::system_clock::now();
        auto end_time = now - std::chrono::hours(1);
        auto start_time = end_time - std::chrono::minutes(duration_minutes);

        int64_t to_ts = std::chrono::duration_cast<std::chrono::milliseconds>(end_time.time_since_epoch()).count();
        int64_t from_ts = std::chrono::duration_cast<std::chrono::milliseconds>(start_time.time_since_epoch()).count();

        std::cout << "Range: " << from_ts << " to " << to_ts << " (" << duration_minutes << " mins)" << std::endl;

        auto candles = client.get_historical_candles(exchange_name, symbol, "1m", from_ts, to_ts);

        // 4. Validate
        if (!candles.empty()) {
             print_candle_vector(candles);

             if (candles.size() < 201) {
                 std::cout << RED << "[FAIL] Received " << candles.size() << " candles. Pagination likely not working (Limit is 200)." << RESET << std::endl;
             } else {
                 std::cout << GREEN << "[PASS] Received " << candles.size() << " candles (Expected > 200)." << RESET << std::endl;
             }
        } else {
            std::cout << YELLOW << "Warning: Returned empty candle vector." << RESET << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << RED << "Test Failed: " << e.what() << RESET << std::endl;
    }

    return 0;
}
