#include "scratchbird/engine/uuid.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace scratchbird::engine
{

    static std::mt19937_64& rng()
    {
        static thread_local std::mt19937_64 gen{std::random_device{}()};
        return gen;
    }

    std::array<std::uint8_t, 16> uuid_v7_bytes()
    {
        // UUIDv7 layout per draft-ietf-uuidrev-rfc4122bis
        // time_ms: 48 bits; version 7 in high 4 bits of byte 6; variant in high bits of byte 8
        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now())
                       .time_since_epoch()
                       .count();
        std::uint64_t time_ms = static_cast<std::uint64_t>(now);
        std::uniform_int_distribution<std::uint64_t> dist;
        std::uint64_t rand64 = dist(rng());
        std::uint32_t rand32 = static_cast<std::uint32_t>(dist(rng()));

        std::array<std::uint8_t, 16> out{};
        // time_ms high 48 bits
        out[0] = (time_ms >> 40) & 0xFF;
        out[1] = (time_ms >> 32) & 0xFF;
        out[2] = (time_ms >> 24) & 0xFF;
        out[3] = (time_ms >> 16) & 0xFF;
        out[4] = (time_ms >> 8) & 0xFF;
        out[5] = (time_ms) & 0xFF;
        // version 7 in high 4 bits of byte 6
        out[6] = static_cast<std::uint8_t>(0x70 | ((rand64 >> 8) & 0x0F));
        // variant 10xx in high bits of byte 8
        out[7] = static_cast<std::uint8_t>(0x80 | (rand64 & 0x3F));
        // remaining 8 bytes random
        out[8] = (rand64 >> 56) & 0xFF;
        out[9] = (rand64 >> 48) & 0xFF;
        out[10] = (rand64 >> 40) & 0xFF;
        out[11] = (rand64 >> 32) & 0xFF;
        out[12] = (rand32 >> 24) & 0xFF;
        out[13] = (rand32 >> 16) & 0xFF;
        out[14] = (rand32 >> 8) & 0xFF;
        out[15] = (rand32) & 0xFF;
        return out;
    }

    std::string uuid_v7_string()
    {
        auto b = uuid_v7_bytes();
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        auto emit = [&](int i) { ss << std::setw(2) << (int)b[i]; };
        for (int i = 0; i < 16; i++)
            b[i] &= 0xFF; // no-op but quiets warnings
        for (int i = 0; i < 16; i++) {
            if (i == 4 || i == 6 || i == 8 || i == 10)
                ss << '-';
            ss << std::setw(2) << std::nouppercase << std::hex << (int)b[i];
        }
        return ss.str();
    }

} // namespace scratchbird::engine
