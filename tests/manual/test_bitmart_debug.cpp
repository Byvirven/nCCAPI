#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

// Include necessary headers
#include "nccapi/sessions/unified_session.hpp"
#include "nccapi/exchanges/bitmart.hpp"
#include "ccapi_cpp/ccapi_session_options.h"
#include "ccapi_cpp/ccapi_session_configs.h"

// Define a simple main function
int main() {
    std::cout << "Starting Bitmart Debug Test..." << std::endl;

    // Initialize session
    ccapi::SessionOptions options;
    ccapi::SessionConfigs configs;
    auto session = std::make_shared<nccapi::UnifiedSession>(options, configs);

    // Create Bitmart exchange instance
    nccapi::Bitmart bitmart(session);

    // Test Parameters
    std::string symbol = "BTC_USDT"; // Common pair
    std::string timeframe = "1h";

    // Current time minus 2 days
    auto now = std::chrono::system_clock::now();
    auto two_days_ago = now - std::chrono::hours(48);

    int64_t to_ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    int64_t from_ts = std::chrono::duration_cast<std::chrono::milliseconds>(two_days_ago.time_since_epoch()).count();

    std::cout << "Fetching candles for " << symbol << " from " << from_ts << " to " << to_ts << std::endl;

    try {
        auto candles = bitmart.get_historical_candles(symbol, timeframe, from_ts, to_ts);

        std::cout << "Received " << candles.size() << " candles." << std::endl;

        for (const auto& c : candles) {
            std::cout << "T: " << c.timestamp << " O: " << c.open << " C: " << c.close << std::endl;
            // Limit output
            if (&c == &candles[4]) break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    std::cout << "Test Finished." << std::endl;
    return 0;
}
