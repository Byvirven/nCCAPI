#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>

#include "nccapi/exchanges/ascendex.hpp"
#include "nccapi/sessions/unified_session.hpp"

using namespace nccapi;

int main() {
    auto session = std::make_shared<UnifiedSession>(
        ccapi::SessionOptions(), ccapi::SessionConfigs()
    );

    Ascendex exchange(session);
    std::string symbol = "BTC/USDT";
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    // 2000 mins > 500 limit
    int64_t from = now - (2000 * 60 * 1000);
    int64_t to = now;

    std::cout << "Testing exchange: ascendex" << std::endl;
    std::cout << "Range: " << from << " to " << to << " (2000 mins)" << std::endl;

    auto candles = exchange.get_historical_candles(symbol, "1m", from, to);

    std::cout << "Received " << candles.size() << " candles." << std::endl;
    if (!candles.empty()) {
        std::cout << "First: " << candles.front().toString() << std::endl;
        std::cout << "Last:  " << candles.back().toString() << std::endl;
    }

    if (candles.size() > 505) {
        std::cout << "\033[32m[PASS] Received " << candles.size() << " candles (Expected > 500).\033[0m" << std::endl;
    } else {
        std::cout << "\033[31m[FAIL] Received " << candles.size() << " candles (Expected > 500).\033[0m" << std::endl;
    }

    session->stop();
    return 0;
}
