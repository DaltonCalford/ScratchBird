#include "scratchbird/sblr/v3_container.h"

#include <cstring>

#include "scratchbird/sblr/v3_codec.h"

namespace scratchbird::sblr::v3 {

static void writeLE16(uint16_t v, std::vector<uint8_t>& out) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

static void writeLE64(uint64_t v, std::vector<uint8_t>& out) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }
}

static bool readLE16(const uint8_t* data, size_t size, size_t& off, uint16_t& out) {
    if (off + 2 > size) return false;
    out = static_cast<uint16_t>(data[off]) | (static_cast<uint16_t>(data[off + 1]) << 8);
    off += 2;
    return true;
}

static bool readLE64(const uint8_t* data, size_t size, size_t& off, uint64_t& out) {
    if (off + 8 > size) return false;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(data[off + i]) << (i * 8);
    }
    out = v;
    off += 8;
    return true;
}

static void padTo8(std::vector<uint8_t>& out) {
    while (out.size() % 8 != 0) {
        out.push_back(0);
    }
}

static void encodeModuleMetadata(const ModuleMetadata& meta, std::vector<uint8_t>& out, std::string& err) {
    DecodeError derr;
    encodeString(meta.module_name, out, derr);
    encodeString(meta.module_version, out, derr);
    writeLE16(meta.dialect_id, out);
    writeLE16(meta.target_platform, out);
    encodeString(meta.build_id, out, derr);
    encodeBytes(meta.source_hash, out, derr);
    (void)err;
}

static bool decodeModuleMetadata(const uint8_t* data, size_t size, size_t& off, ModuleMetadata& out, std::string& err) {
    DecodeError derr;
    if (!decodeString(data, size, off, out.module_name, derr)) { err = derr.message; return false; }
    if (!decodeString(data, size, off, out.module_version, derr)) { err = derr.message; return false; }
    if (!readLE16(data, size, off, out.dialect_id)) { err = "dialect_id"; return false; }
    if (!readLE16(data, size, off, out.target_platform)) { err = "target_platform"; return false; }
    if (!decodeString(data, size, off, out.build_id, derr)) { err = derr.message; return false; }
    if (!decodeBytes(data, size, off, out.source_hash, derr)) { err = derr.message; return false; }
    return true;
}

static void encodeSymbolTable(const std::vector<std::string>& symbols, std::vector<uint8_t>& out) {
    encodeVaruint(symbols.size(), out);
    DecodeError derr;
    for (const auto& s : symbols) {
        encodeString(s, out, derr);
    }
}

static bool decodeSymbolTable(const uint8_t* data, size_t size, size_t& off, std::vector<std::string>& out, std::string& err) {
    uint64_t count = 0;
    if (!decodeVaruint(data, size, off, count)) { err = "symbol_count"; return false; }
    DecodeError derr;
    out.clear();
    out.reserve(static_cast<size_t>(count));
    for (size_t i = 0; i < count; ++i) {
        std::string s;
        if (!decodeString(data, size, off, s, derr)) { err = derr.message; return false; }
        out.push_back(std::move(s));
    }
    return true;
}

static void encodeConstantPool(const std::vector<ConstantPoolEntry>& pool, std::vector<uint8_t>& out, std::string& err) {
    encodeVaruint(pool.size(), out);
    DecodeError derr;
    for (const auto& entry : pool) {
        out.push_back(entry.tag);
        switch (entry.tag) {
            case 0x01: { // int64
                auto v = std::get_if<int64_t>(&entry.value.data);
                if (!v) { err = "const int64 missing"; return; }
                writeLE64(static_cast<uint64_t>(*v), out);
                break;
            }
            case 0x02: { // uint64
                auto v = std::get_if<uint64_t>(&entry.value.data);
                if (!v) { err = "const uint64 missing"; return; }
                writeLE64(*v, out);
                break;
            }
            case 0x03: { // float64
                auto v = std::get_if<double>(&entry.value.data);
                if (!v) { err = "const float64 missing"; return; }
                uint64_t raw;
                std::memcpy(&raw, v, sizeof(raw));
                writeLE64(raw, out);
                break;
            }
            case 0x04: { // string
                auto v = std::get_if<uint64_t>(&entry.value.data);
                if (!v) { err = "const string_id missing"; return; }
                encodeVaruint(*v, out);
                break;
            }
            case 0x05: { // bytes
                auto v = std::get_if<Value::Bytes>(&entry.value.data);
                if (!v) { err = "const bytes missing"; return; }
                encodeBytes(*v, out, derr);
                break;
            }
            case 0x06: { // uuid
                auto v = std::get_if<Value::Bytes>(&entry.value.data);
                if (!v || v->size() != 16) { err = "const uuid missing"; return; }
                out.insert(out.end(), v->begin(), v->end());
                break;
            }
            case 0x07: { // decimal
                auto obj = std::get_if<Value::Object>(&entry.value.data);
                if (!obj) { err = "const decimal missing"; return; }
                auto scale_it = obj->find("scale");
                auto bytes_it = obj->find("bcd");
                if (scale_it == obj->end() || bytes_it == obj->end()) { err = "const decimal fields"; return; }
                int64_t scale = std::get<int64_t>(scale_it->second.data);
                writeLE16(static_cast<uint16_t>(scale & 0xFFFF), out);
                writeLE16(static_cast<uint16_t>((scale >> 16) & 0xFFFF), out);
                auto b = std::get<Value::Bytes>(bytes_it->second.data);
                encodeVaruint(b.size(), out);
                out.insert(out.end(), b.begin(), b.end());
                break;
            }
            case 0x08: { // boolean
                auto v = std::get_if<bool>(&entry.value.data);
                if (!v) { err = "const bool missing"; return; }
                out.push_back(*v ? 1 : 0);
                break;
            }
            case 0x09: { // typed null
                auto v = std::get_if<uint64_t>(&entry.value.data);
                if (!v) { err = "const null type missing"; return; }
                encodeVaruint(*v, out);
                break;
            }
            default:
                err = "unknown constant tag";
                return;
        }
    }
}

static bool decodeConstantPool(const uint8_t* data, size_t size, size_t& off, std::vector<ConstantPoolEntry>& out, std::string& err) {
    uint64_t count = 0;
    if (!decodeVaruint(data, size, off, count)) { err = "pool_count"; return false; }
    out.clear();
    out.reserve(static_cast<size_t>(count));
    for (size_t i = 0; i < count; ++i) {
        if (off >= size) { err = "pool tag"; return false; }
        uint8_t tag = data[off++];
        ConstantPoolEntry entry; entry.tag = tag;
        switch (tag) {
            case 0x01: { uint64_t v; if (!readLE64(data, size, off, v)) { err = "int64"; return false; } entry.value = Value(static_cast<int64_t>(v)); break; }
            case 0x02: { uint64_t v; if (!readLE64(data, size, off, v)) { err = "uint64"; return false; } entry.value = Value(v); break; }
            case 0x03: { uint64_t raw; if (!readLE64(data, size, off, raw)) { err = "float64"; return false; } double d; std::memcpy(&d, &raw, sizeof(d)); entry.value = Value(d); break; }
            case 0x04: { uint64_t id; if (!decodeVaruint(data, size, off, id)) { err = "string_id"; return false; } entry.value = Value(id); break; }
            case 0x05: { Value::Bytes b; DecodeError derr; if (!decodeBytes(data, size, off, b, derr)) { err = derr.message; return false; } entry.value = Value(std::move(b)); break; }
            case 0x06: { if (off + 16 > size) { err = "uuid"; return false; } Value::Bytes b(data + off, data + off + 16); off += 16; entry.value = Value(std::move(b)); break; }
            case 0x07: { int32_t scale_low=0, scale_high=0; if (!readLE16(data, size, off, *reinterpret_cast<uint16_t*>(&scale_low)) || !readLE16(data, size, off, *reinterpret_cast<uint16_t*>(&scale_high))) { err = "decimal scale"; return false; } int32_t scale = (scale_high << 16) | (scale_low & 0xFFFF); uint64_t len; if (!decodeVaruint(data, size, off, len)) { err = "decimal len"; return false; } if (off + len > size) { err = "decimal bytes"; return false; } Value::Bytes b(data + off, data + off + len); off += len; Value::Object obj; obj["scale"] = Value(static_cast<int64_t>(scale)); obj["bcd"] = Value(std::move(b)); entry.value = Value(std::move(obj)); break; }
            case 0x08: { if (off >= size) { err = "bool"; return false; } entry.value = Value(data[off++] != 0); break; }
            case 0x09: { uint64_t type_id; if (!decodeVaruint(data, size, off, type_id)) { err = "null type"; return false; } entry.value = Value(type_id); break; }
            default: err = "unknown constant tag"; return false;
        }
        out.push_back(std::move(entry));
    }
    return true;
}

bool encodeContainer(const Container& container, std::vector<uint8_t>& out, std::string& err) {
    ContainerHeader header = container.header;
    std::vector<uint8_t> module_data, symbols_data, constants_data;
    std::vector<uint8_t> bytecode_data = container.bytecode_stream;
    std::vector<uint8_t> deps = container.dependencies;
    std::vector<uint8_t> debug = container.debug_info;
    std::vector<uint8_t> integrity = container.integrity;

    encodeModuleMetadata(container.metadata, module_data, err);
    encodeSymbolTable(container.symbols, symbols_data);
    encodeConstantPool(container.constants, constants_data, err);

    std::vector<SectionEntry> sections;
    sections.reserve(4);
    auto addSection = [&](uint16_t id, const std::vector<uint8_t>& data, uint16_t flags) {
        SectionEntry e{}; e.section_id = id; e.section_flags = flags; e.offset = 0; e.length = data.size();
        sections.push_back(e);
    };
    addSection(SECTION_MODULE_METADATA, module_data, 0);
    addSection(SECTION_SYMBOL_TABLE, symbols_data, 0);
    addSection(SECTION_CONSTANT_POOL, constants_data, 0);
    addSection(SECTION_BYTECODE_STREAM, bytecode_data, 0);
    if (!deps.empty()) addSection(SECTION_DEPENDENCIES, deps, 0);
    if (!debug.empty()) addSection(SECTION_DEBUG_INFO, debug, 0);
    if (!integrity.empty()) addSection(SECTION_INTEGRITY, integrity, 0);

    header.section_count = static_cast<uint16_t>(sections.size());

    std::vector<uint8_t> header_bytes;
    header_bytes.insert(header_bytes.end(), {'S','B','L','3'});
    writeLE16(header.version_major, header_bytes);
    writeLE16(header.version_minor, header_bytes);
    writeLE16(header.version_patch, header_bytes);
    writeLE16(header.flags, header_bytes);
    writeLE16(header.section_count, header_bytes);
    writeLE16(0, header_bytes);  // header_size placeholder
    writeLE64(0, header_bytes);  // container_size placeholder
    writeLE64(header.timestamp_utc, header_bytes);
    for (int i = 0; i < 16; ++i) header_bytes.push_back(header.module_id[i]);

    size_t section_table_offset = header_bytes.size();
    for (const auto& s : sections) {
        writeLE16(s.section_id, header_bytes);
        writeLE16(s.section_flags, header_bytes);
        writeLE64(0, header_bytes); // offset placeholder
        writeLE64(s.length, header_bytes);
    }

    size_t header_size = header_bytes.size();
    while (header_size % 8 != 0) { header_bytes.push_back(0); header_size++; }

    std::vector<uint8_t> body;
    body.reserve(module_data.size() + symbols_data.size() + constants_data.size() + bytecode_data.size());

    auto writeSection = [&](SectionEntry& s, const std::vector<uint8_t>& data) {
        padTo8(body);
        s.offset = header_size + body.size();
        body.insert(body.end(), data.begin(), data.end());
    };

    writeSection(sections[0], module_data);
    writeSection(sections[1], symbols_data);
    writeSection(sections[2], constants_data);
    writeSection(sections[3], bytecode_data);
    size_t idx = 4;
    if (!deps.empty()) writeSection(sections[idx++], deps);
    if (!debug.empty()) writeSection(sections[idx++], debug);
    if (!integrity.empty()) writeSection(sections[idx++], integrity);

    // patch header_size and container_size
    size_t container_size = header_size + body.size();

    // patch header bytes
    header_bytes[12] = static_cast<uint8_t>(header_size & 0xFF);
    header_bytes[13] = static_cast<uint8_t>((header_size >> 8) & 0xFF);
    uint64_t cs = static_cast<uint64_t>(container_size);
    for (int i = 0; i < 8; ++i) {
        header_bytes[14 + i] = static_cast<uint8_t>((cs >> (i * 8)) & 0xFF);
    }

    // patch section offsets
    size_t table_off = section_table_offset;
    for (const auto& s : sections) {
        table_off += 4; // id+flags
        for (int i = 0; i < 8; ++i) {
            header_bytes[table_off + i] = static_cast<uint8_t>((s.offset >> (i * 8)) & 0xFF);
        }
        table_off += 8; // offset
        table_off += 8; // length
    }

    out.clear();
    out.insert(out.end(), header_bytes.begin(), header_bytes.end());
    out.insert(out.end(), body.begin(), body.end());
    return true;
}

bool decodeContainer(const uint8_t* data, size_t size, Container& out, std::string& err) {
    if (size < 4) { err = "container too small"; return false; }
    if (std::memcmp(data, "SBL3", 4) != 0) { err = "bad magic"; return false; }
    size_t off = 4;
    if (!readLE16(data, size, off, out.header.version_major) ||
        !readLE16(data, size, off, out.header.version_minor) ||
        !readLE16(data, size, off, out.header.version_patch) ||
        !readLE16(data, size, off, out.header.flags) ||
        !readLE16(data, size, off, out.header.section_count) ||
        !readLE16(data, size, off, out.header.header_size) ||
        !readLE64(data, size, off, out.header.container_size) ||
        !readLE64(data, size, off, out.header.timestamp_utc)) {
        err = "header parse";
        return false;
    }
    if (off + 16 > size) { err = "module_id"; return false; }
    std::memcpy(out.header.module_id, data + off, 16); off += 16;

    if (out.header.header_size > size) { err = "header_size"; return false; }

    out.sections.clear();
    for (uint16_t i = 0; i < out.header.section_count; ++i) {
        SectionEntry s{};
        if (!readLE16(data, size, off, s.section_id) ||
            !readLE16(data, size, off, s.section_flags) ||
            !readLE64(data, size, off, s.offset) ||
            !readLE64(data, size, off, s.length)) {
            err = "section table";
            return false;
        }
        out.sections.push_back(s);
    }

    auto readSection = [&](uint16_t id, std::vector<uint8_t>& dest) -> bool {
        for (const auto& s : out.sections) {
            if (s.section_id == id) {
                if (s.offset + s.length > size) { err = "section bounds"; return false; }
                dest.assign(data + s.offset, data + s.offset + s.length);
                return true;
            }
        }
        return false;
    };

    std::vector<uint8_t> module_data, symbols_data, constants_data;
    if (!readSection(SECTION_MODULE_METADATA, module_data) ||
        !readSection(SECTION_SYMBOL_TABLE, symbols_data) ||
        !readSection(SECTION_CONSTANT_POOL, constants_data) ||
        !readSection(SECTION_BYTECODE_STREAM, out.bytecode_stream)) {
        err = "missing required section";
        return false;
    }

    size_t tmp = 0;
    if (!decodeModuleMetadata(module_data.data(), module_data.size(), tmp, out.metadata, err)) return false;
    tmp = 0;
    if (!decodeSymbolTable(symbols_data.data(), symbols_data.size(), tmp, out.symbols, err)) return false;
    tmp = 0;
    if (!decodeConstantPool(constants_data.data(), constants_data.size(), tmp, out.constants, err)) return false;

    readSection(SECTION_DEPENDENCIES, out.dependencies);
    readSection(SECTION_DEBUG_INFO, out.debug_info);
    readSection(SECTION_INTEGRITY, out.integrity);
    return true;
}

}  // namespace scratchbird::sblr::v3
