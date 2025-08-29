#include "scratchbird/engine/parser.h"
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    using namespace scratchbird::engine;
    if (size == 0 || size > 1 << 20) return 0;
    std::string s(reinterpret_cast<const char*>(data), size);
    Status st{};
    // Parse only; ignore semantics
    Parser p;
    (void)p.parse_sql(s, st);
    return 0;
}

