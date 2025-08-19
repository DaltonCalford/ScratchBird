#ifndef SCRATCHBIRD_ENGINE_CONSTANTS_H
#define SCRATCHBIRD_ENGINE_CONSTANTS_H

#include <array>
#include <cstdint>

namespace scratchbird::engine
{

    inline constexpr std::array<std::uint32_t, 6> kAllowedPageSizesBytes = {
        4096u, 8192u, 16384u, 32768u, 65536u, 131072u};

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_CONSTANTS_H
