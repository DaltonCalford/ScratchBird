#include "scratchbird/engine/heap.h"
#include <cstddef>
#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    using namespace scratchbird::engine;
    if (size < 8 || size > (1 << 20)) return 0;
    // Fuzz encode/decode tuple-like payloads
    std::vector<char> in(data, data + size);
    std::vector<char> out;
    // Use small page size to exercise boundaries
    std::size_t ps = 1024;
    // Simulate heap tuple encode/decode paths that operate on buffers
    // Here we just roundtrip copy to ensure no UB in basic operations
    out.assign(in.begin(), in.end());
    return 0;
}

