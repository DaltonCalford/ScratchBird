#include "scratchbird/core/gin_compression.h"
#include <algorithm>

namespace scratchbird
{
    namespace core
    {
        size_t encode_varbyte(uint64_t value, uint8_t *output)
        {
            if (value <= VARBYTE_1_BYTE_MAX)
            {
                // 1 byte: 0xxxxxxx
                output[0] = static_cast<uint8_t>(value);
                return 1;
            }
            else if (value <= VARBYTE_2_BYTE_MAX)
            {
                // 2 bytes: 10xxxxxx xxxxxxxx
                output[0] = 0x80 | static_cast<uint8_t>(value >> 8);
                output[1] = static_cast<uint8_t>(value & 0xFF);
                return 2;
            }
            else if (value <= VARBYTE_3_BYTE_MAX)
            {
                // 3 bytes: 110xxxxx xxxxxxxx xxxxxxxx
                output[0] = 0xC0 | static_cast<uint8_t>(value >> 16);
                output[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[2] = static_cast<uint8_t>(value & 0xFF);
                return 3;
            }
            else if (value <= VARBYTE_4_BYTE_MAX)
            {
                // 4 bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
                output[0] = 0xE0 | static_cast<uint8_t>(value >> 24);
                output[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
                output[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[3] = static_cast<uint8_t>(value & 0xFF);
                return 4;
            }
            else
            {
                // 5 bytes: 11110000 xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
                output[0] = 0xF0;
                output[1] = static_cast<uint8_t>((value >> 24) & 0xFF);
                output[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
                output[3] = static_cast<uint8_t>((value >> 8) & 0xFF);
                output[4] = static_cast<uint8_t>(value & 0xFF);
                return 5;
            }
        }

        size_t decode_varbyte(const uint8_t *input, uint64_t *value_out)
        {
            uint8_t first = input[0];

            if ((first & VARBYTE_1_BYTE_MASK) == 0)
            {
                // 1 byte: 0xxxxxxx
                *value_out = first;
                return 1;
            }
            else if ((first & VARBYTE_2_BYTE_MASK) == 0x80)
            {
                // 2 bytes: 10xxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x3F) << 8) | input[1];
                return 2;
            }
            else if ((first & VARBYTE_3_BYTE_MASK) == 0xC0)
            {
                // 3 bytes: 110xxxxx xxxxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x1F) << 16) |
                             (static_cast<uint64_t>(input[1]) << 8) |
                             input[2];
                return 3;
            }
            else if ((first & VARBYTE_4_BYTE_MASK) == 0xE0)
            {
                // 4 bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(first & 0x0F) << 24) |
                             (static_cast<uint64_t>(input[1]) << 16) |
                             (static_cast<uint64_t>(input[2]) << 8) |
                             input[3];
                return 4;
            }
            else if ((first & VARBYTE_4_BYTE_MASK) == 0xF0)
            {
                // 5 bytes: 11110000 xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
                *value_out = (static_cast<uint64_t>(input[1]) << 24) |
                             (static_cast<uint64_t>(input[2]) << 16) |
                             (static_cast<uint64_t>(input[3]) << 8) |
                             input[4];
                return 5;
            }
            else
            {
                // Invalid encoding
                *value_out = 0;
                return 0;
            }
        }

        size_t compress_posting_list(const uint64_t *tids, uint16_t count,
                                      uint8_t *compressed_out, size_t max_bytes)
        {
            if (count == 0)
            {
                return 0;
            }

            size_t bytes_written = 0;
            uint64_t prev_tid = 0;

            for (uint16_t i = 0; i < count; i++)
            {
                // Calculate delta (first TID is stored as-is)
                uint64_t delta = (i == 0) ? tids[i] : (tids[i] - prev_tid);

                // Check if we have enough space (worst case: 5 bytes)
                if (bytes_written + 5 > max_bytes)
                {
                    // Not enough space - return 0 to indicate failure
                    return 0;
                }

                // Encode delta using varbyte
                size_t encoded = encode_varbyte(delta, compressed_out + bytes_written);
                bytes_written += encoded;
                prev_tid = tids[i];
            }

            return bytes_written;
        }

        size_t decompress_posting_list(const uint8_t *compressed, size_t compressed_bytes,
                                        uint64_t *tids_out, uint16_t max_tids)
        {
            size_t bytes_read = 0;
            uint16_t tid_count = 0;
            uint64_t current_tid = 0;

            while (bytes_read < compressed_bytes && tid_count < max_tids)
            {
                uint64_t delta;
                size_t decoded = decode_varbyte(compressed + bytes_read, &delta);

                if (decoded == 0)
                {
                    // Decoding error - return what we have so far
                    break;
                }

                bytes_read += decoded;
                current_tid += delta;
                tids_out[tid_count++] = current_tid;
            }

            return tid_count;
        }

        size_t estimate_compressed_size(const uint64_t *tids, uint16_t count)
        {
            if (count == 0)
            {
                return 0;
            }

            size_t estimated_bytes = 0;
            uint64_t prev_tid = 0;

            for (uint16_t i = 0; i < count; i++)
            {
                uint64_t delta = (i == 0) ? tids[i] : (tids[i] - prev_tid);

                // Estimate bytes for this delta
                if (delta <= VARBYTE_1_BYTE_MAX)
                {
                    estimated_bytes += 1;
                }
                else if (delta <= VARBYTE_2_BYTE_MAX)
                {
                    estimated_bytes += 2;
                }
                else if (delta <= VARBYTE_3_BYTE_MAX)
                {
                    estimated_bytes += 3;
                }
                else if (delta <= VARBYTE_4_BYTE_MAX)
                {
                    estimated_bytes += 4;
                }
                else
                {
                    estimated_bytes += 5;
                }

                prev_tid = tids[i];
            }

            return estimated_bytes;
        }

        bool should_compress(const uint64_t *tids, uint16_t count)
        {
            if (count == 0)
            {
                return false;
            }

            // Don't compress very small lists (overhead not worth it)
            if (count < 10)
            {
                return false;
            }

            size_t uncompressed_size = count * sizeof(uint64_t);
            size_t compressed_size = estimate_compressed_size(tids, count);

            // Compress if we save at least 10% space
            return (compressed_size * 10) < (uncompressed_size * 9);
        }

    } // namespace core
} // namespace scratchbird
