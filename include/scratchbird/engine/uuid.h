#ifndef SCRATCHBIRD_ENGINE_UUID_H
#define SCRATCHBIRD_ENGINE_UUID_H

#include <array>
#include <cstdint>
#include <string>

namespace scratchbird::engine
{

    // Returns 16-byte UUID (version 7, sortable by time).
    std::array<std::uint8_t, 16> uuid_v7_bytes();

    // Returns canonical 36-char textual representation (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx).
    std::string uuid_v7_string();

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_UUID_H
