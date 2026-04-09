#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "scratchbird/sblr/v3_types.h"

namespace scratchbird::sblr::v3 {

struct ContainerHeader {
    char magic[4];
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t version_patch;
    uint16_t flags;
    uint16_t section_count;
    uint16_t header_size;
    uint64_t container_size;
    uint64_t timestamp_utc;
    uint8_t module_id[16];
};

struct SectionEntry {
    uint16_t section_id;
    uint16_t section_flags;
    uint64_t offset;
    uint64_t length;
};

enum SectionId : uint16_t {
    SECTION_MODULE_METADATA = 0x0001,
    SECTION_SYMBOL_TABLE = 0x0002,
    SECTION_CONSTANT_POOL = 0x0003,
    SECTION_BYTECODE_STREAM = 0x0004,
    SECTION_DEPENDENCIES = 0x0005,
    SECTION_DEBUG_INFO = 0x0006,
    SECTION_INTEGRITY = 0x0007,
    SECTION_RETAINED_SYMBOLS = 0x0008
};

struct ModuleMetadata {
    std::string module_name;
    std::string module_version;
    uint16_t dialect_id = 0;
    uint16_t target_platform = 0;
    std::string build_id;
    std::vector<uint8_t> source_hash;
};

struct ConstantPoolEntry {
    uint8_t tag = 0;
    Value value;
};

struct Container {
    ContainerHeader header{};
    std::vector<SectionEntry> sections;
    ModuleMetadata metadata;
    std::vector<std::string> symbols;
    Value::Object retained_symbol_payload;
    std::vector<ConstantPoolEntry> constants;
    std::vector<uint8_t> bytecode_stream;
    std::vector<uint8_t> dependencies;
    std::vector<uint8_t> debug_info;
    std::vector<uint8_t> integrity;
};

bool encodeContainer(const Container& container, std::vector<uint8_t>& out, std::string& err);
bool decodeContainer(const uint8_t* data, size_t size, Container& out, std::string& err);
Value::Object buildNormalizedRetainedSymbolPayload(const Instruction& root);

}  // namespace scratchbird::sblr::v3
