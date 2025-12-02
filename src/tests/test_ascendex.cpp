#include "common.hpp"
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    bool verbose = false;
    std::vector<std::string> args(argv, argv + argc);
    for (const auto& arg : args) {
        if (arg == "--verbose") {
            verbose = true;
        }
    }
    run_exchange_test("ascendex", verbose);
    return 0;
}
