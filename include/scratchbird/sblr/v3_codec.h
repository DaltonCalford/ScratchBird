#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "scratchbird/sblr/v3_schema.h"
#include "scratchbird/sblr/v3_types.h"

namespace scratchbird::sblr::v3 {

struct DecodeError {
    std::string message;
};

using Buffer = std::vector<uint8_t>;

// Varuint (ULEB128)
void encodeVaruint(uint64_t value, Buffer& out);
bool decodeVaruint(const uint8_t* data, size_t size, size_t& offset, uint64_t& out);

// Strings/bytes
bool encodeString(const std::string& s, Buffer& out, DecodeError& err);
bool decodeString(const uint8_t* data, size_t size, size_t& offset, std::string& out, DecodeError& err);
bool encodeBytes(const std::vector<uint8_t>& b, Buffer& out, DecodeError& err);
bool decodeBytes(const uint8_t* data, size_t size, size_t& offset, std::vector<uint8_t>& out, DecodeError& err);

// Instruction header encoding
void encodeInstruction(const Instruction& inst, Buffer& out);
bool decodeInstruction(const uint8_t* data, size_t size, size_t& offset, Instruction& out, DecodeError& err);

// Payload encoding/decoding using schema registry
bool encodePayloadBySchema(const SchemaDef& schema, const Value& payload, Buffer& out, DecodeError& err);
bool decodePayloadBySchema(const SchemaDef& schema, const uint8_t* data, size_t size, size_t& offset, Value& out, DecodeError& err);

// Helpers
bool encodeValue(const FieldDef& field, const Value& value, Buffer& out, DecodeError& err);
bool decodeValue(const FieldDef& field, const uint8_t* data, size_t size, size_t& offset, Value& out, DecodeError& err);

}  // namespace scratchbird::sblr::v3
