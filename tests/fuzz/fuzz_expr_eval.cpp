#include "scratchbird/engine/expr.h"
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    using namespace scratchbird::engine;
    if (size == 0 || size > 1 << 20) return 0;
    std::string s(reinterpret_cast<const char*>(data), size);
    Status st{};
    // Attempt to parse an expression; ensure no crashes
    (void)parse_expression(s, st);
    return 0;
}

