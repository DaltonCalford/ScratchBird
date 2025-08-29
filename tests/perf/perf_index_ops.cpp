#include "scratchbird/engine.h"
#include <chrono>
#include <iostream>

int main() {
    using namespace scratchbird;
    Status st{};
    auto start = std::chrono::high_resolution_clock::now();
    // placeholder for index ops benchmark
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "index_ops_ms " << ms << "\n";
    return 0;
}

