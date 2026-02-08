#include "scratchbird/sblr/v3_codec.h"

#include <cstring>
#include <limits>

namespace scratchbird::sblr::v3 {

static void writeBytes(const uint8_t* data, size_t len, Buffer& out) {
    out.insert(out.end(), data, data + len);
}

template <typename T>
static void writeLE(T value, Buffer& out) {
    static_assert(std::is_integral<T>::value, "integral only");
    for (size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<uint8_t>((static_cast<uint64_t>(value) >> (i * 8)) & 0xFF));
    }
}

template <typename T>
static bool readLE(const uint8_t* data, size_t size, size_t& offset, T& out) {
    if (offset + sizeof(T) > size) {
        return false;
    }
    uint64_t v = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        v |= (static_cast<uint64_t>(data[offset + i]) << (i * 8));
    }
    offset += sizeof(T);
    out = static_cast<T>(v);
    return true;
}

void encodeVaruint(uint64_t value, Buffer& out) {
    while (value >= 0x80) {
        out.push_back(static_cast<uint8_t>(value | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

bool decodeVaruint(const uint8_t* data, size_t size, size_t& offset, uint64_t& out) {
    uint64_t result = 0;
    uint32_t shift = 0;
    while (true) {
        if (offset >= size || shift > 63) {
            return false;
        }
        uint8_t byte = data[offset++];
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            break;
        }
        shift += 7;
    }
    out = result;
    return true;
}

bool encodeString(const std::string& s, Buffer& out, DecodeError& err) {
    if (s.size() > std::numeric_limits<uint32_t>::max()) {
        err.message = "string too long";
        return false;
    }
    encodeVaruint(static_cast<uint64_t>(s.size()), out);
    writeBytes(reinterpret_cast<const uint8_t*>(s.data()), s.size(), out);
    return true;
}

bool decodeString(const uint8_t* data, size_t size, size_t& offset, std::string& out, DecodeError& err) {
    uint64_t len = 0;
    if (!decodeVaruint(data, size, offset, len)) {
        err.message = "invalid string length";
        return false;
    }
    if (offset + len > size) {
        err.message = "string exceeds buffer";
        return false;
    }
    out.assign(reinterpret_cast<const char*>(data + offset), static_cast<size_t>(len));
    offset += static_cast<size_t>(len);
    return true;
}

bool encodeBytes(const std::vector<uint8_t>& b, Buffer& out, DecodeError& err) {
    if (b.size() > std::numeric_limits<uint32_t>::max()) {
        err.message = "bytes too long";
        return false;
    }
    encodeVaruint(static_cast<uint64_t>(b.size()), out);
    writeBytes(b.data(), b.size(), out);
    return true;
}

bool decodeBytes(const uint8_t* data, size_t size, size_t& offset, std::vector<uint8_t>& out, DecodeError& err) {
    uint64_t len = 0;
    if (!decodeVaruint(data, size, offset, len)) {
        err.message = "invalid bytes length";
        return false;
    }
    if (offset + len > size) {
        err.message = "bytes exceed buffer";
        return false;
    }
    out.assign(data + offset, data + offset + len);
    offset += static_cast<size_t>(len);
    return true;
}

void encodeInstruction(const Instruction& inst, Buffer& out) {
    Buffer payload;
    DecodeError err;
    if (!std::holds_alternative<Value::Object>(inst.payload.data)) {
        // treat non-object payload as raw bytes (if provided)
        if (auto bytes = std::get_if<Value::Bytes>(&inst.payload.data)) {
            payload = *bytes;
        }
    } else {
        // lookup schema by opcode name handled by caller; payload encoding done elsewhere
        // here we just treat payload as already encoded in inst.payload bytes if present
        // so the caller should use encodePayloadBySchema
    }

    writeLE<uint16_t>(inst.opcode, out);
    writeLE<uint16_t>(inst.flags, out);
    writeLE<uint32_t>(static_cast<uint32_t>(payload.size()), out);
    out.insert(out.end(), payload.begin(), payload.end());
}

bool decodeInstruction(const uint8_t* data, size_t size, size_t& offset, Instruction& out, DecodeError& err) {
    uint16_t opcode = 0;
    uint16_t flags = 0;
    uint32_t payload_len = 0;
    if (!readLE<uint16_t>(data, size, offset, opcode) ||
        !readLE<uint16_t>(data, size, offset, flags) ||
        !readLE<uint32_t>(data, size, offset, payload_len)) {
        err.message = "invalid instruction header";
        return false;
    }
    if (offset + payload_len > size) {
        err.message = "payload exceeds buffer";
        return false;
    }
    out.opcode = opcode;
    out.flags = flags;
    out.payload = Value(Value::Bytes{data + offset, data + offset + payload_len});
    offset += payload_len;
    return true;
}


static Value defaultValueForField(const FieldDef& field) {
    switch (field.type) {
        case FieldType::U8:
        case FieldType::U16:
        case FieldType::U32:
        case FieldType::U64:
        case FieldType::VARUINT:
            return Value(uint64_t(0));
        case FieldType::I8:
        case FieldType::I16:
        case FieldType::I32:
        case FieldType::I64:
            return Value(int64_t(0));
        case FieldType::U128:
        case FieldType::UUID:
            return Value(Value::Bytes(16, 0));
        case FieldType::F32:
        case FieldType::F64:
            return Value(double(0.0));
        case FieldType::BOOL:
            return Value(false);
        case FieldType::STRING:
        case FieldType::IDENT:
            return Value(std::string());
        case FieldType::BYTES:
            return Value(Value::Bytes());
        case FieldType::SCHEMA_PATH:
        case FieldType::EXPR_LIST:
        case FieldType::STMT_LIST:
        case FieldType::LIST:
            return Value(Value::List());
        case FieldType::TYPE_SPEC:
            return Value(TypeSpec{});
        case FieldType::OPT:
            return Value();
        case FieldType::SCHEMA:
            return Value(Value::Object());
        case FieldType::EXPR:
        case FieldType::STMT:
            return Value();
    }
    return Value();
}

bool encodePayloadBySchema(const SchemaDef& schema, const Value& payload, Buffer& out, DecodeError& err) {
    if (!std::holds_alternative<Value::Object>(payload.data)) {
        err.message = "payload is not object";
        return false;
    }
    const auto& obj = std::get<Value::Object>(payload.data);
    for (const auto& field : schema.fields) {
        auto it = obj.find(field.name);
        Value value;
        if (it != obj.end()) {
            value = it->second;
        } else {
            value = defaultValueForField(field);
        }
        if (!encodeValue(field, value, out, err)) {
            return false;
        }
    }
    return true;
}

bool decodePayloadBySchema(const SchemaDef& schema, const uint8_t* data, size_t size, size_t& offset, Value& out, DecodeError& err) {
    Value::Object obj;
    for (const auto& field : schema.fields) {
        Value value;
        if (!decodeValue(field, data, size, offset, value, err)) {
            return false;
        }
        obj[field.name] = std::move(value);
    }
    out = Value(std::move(obj));
    return true;
}

static bool encodeList(const FieldDef& field, const Value::List& list, Buffer& out, DecodeError& err) {
    encodeVaruint(static_cast<uint64_t>(list.size()), out);
    FieldDef inner{field.name, FieldType::SCHEMA, field.ref};
    for (const auto& v : list) {
        if (!encodeValue(inner, v, out, err)) {
            return false;
        }
    }
    return true;
}

bool encodeValue(const FieldDef& field, const Value& value, Buffer& out, DecodeError& err) {
    switch (field.type) {
        case FieldType::U8: {
            auto v = std::get_if<uint64_t>(&value.data);
            if (!v) { err.message = "expected u8"; return false; }
            out.push_back(static_cast<uint8_t>(*v));
            return true;
        }
        case FieldType::U16: {
            auto v = std::get_if<uint64_t>(&value.data);
            if (!v) { err.message = "expected u16"; return false; }
            writeLE<uint16_t>(static_cast<uint16_t>(*v), out);
            return true;
        }
        case FieldType::U32: {
            auto v = std::get_if<uint64_t>(&value.data);
            if (!v) { err.message = "expected u32"; return false; }
            writeLE<uint32_t>(static_cast<uint32_t>(*v), out);
            return true;
        }
        case FieldType::U64: {
            auto v = std::get_if<uint64_t>(&value.data);
            if (!v) { err.message = "expected u64"; return false; }
            writeLE<uint64_t>(*v, out);
            return true;
        }
        case FieldType::I8: {
            auto v = std::get_if<int64_t>(&value.data);
            if (!v) { err.message = "expected i8"; return false; }
            out.push_back(static_cast<uint8_t>(*v & 0xFF));
            return true;
        }
        case FieldType::I16: {
            auto v = std::get_if<int64_t>(&value.data);
            if (!v) { err.message = "expected i16"; return false; }
            writeLE<int16_t>(static_cast<int16_t>(*v), out);
            return true;
        }
        case FieldType::I32: {
            auto v = std::get_if<int64_t>(&value.data);
            if (!v) { err.message = "expected i32"; return false; }
            writeLE<int32_t>(static_cast<int32_t>(*v), out);
            return true;
        }
        case FieldType::I64: {
            auto v = std::get_if<int64_t>(&value.data);
            if (!v) { err.message = "expected i64"; return false; }
            writeLE<int64_t>(*v, out);
            return true;
        }
        case FieldType::U128:
        case FieldType::UUID: {
            auto b = std::get_if<Value::Bytes>(&value.data);
            if (!b || b->size() != 16) { err.message = "expected 16-byte value"; return false; }
            writeBytes(b->data(), 16, out);
            return true;
        }
        case FieldType::F32: {
            auto v = std::get_if<double>(&value.data);
            if (!v) { err.message = "expected f32"; return false; }
            float f = static_cast<float>(*v);
            uint32_t raw;
            std::memcpy(&raw, &f, sizeof(raw));
            writeLE<uint32_t>(raw, out);
            return true;
        }
        case FieldType::F64: {
            auto v = std::get_if<double>(&value.data);
            if (!v) { err.message = "expected f64"; return false; }
            uint64_t raw;
            std::memcpy(&raw, v, sizeof(raw));
            writeLE<uint64_t>(raw, out);
            return true;
        }
        case FieldType::BOOL: {
            auto v = std::get_if<bool>(&value.data);
            if (!v) { err.message = "expected bool"; return false; }
            out.push_back(*v ? 1 : 0);
            return true;
        }
        case FieldType::VARUINT: {
            auto v = std::get_if<uint64_t>(&value.data);
            if (!v) { err.message = "expected varuint"; return false; }
            encodeVaruint(*v, out);
            return true;
        }
        case FieldType::STRING:
        case FieldType::IDENT: {
            auto v = std::get_if<std::string>(&value.data);
            if (!v) { err.message = "expected string"; return false; }
            return encodeString(*v, out, err);
        }
        case FieldType::BYTES: {
            auto v = std::get_if<Value::Bytes>(&value.data);
            if (!v) { err.message = "expected bytes"; return false; }
            return encodeBytes(*v, out, err);
        }
        case FieldType::SCHEMA_PATH: {
            auto v = std::get_if<Value::List>(&value.data);
            if (!v) { err.message = "expected schema_path list"; return false; }
            encodeVaruint(static_cast<uint64_t>(v->size()), out);
            for (const auto& item : *v) {
                auto s = std::get_if<std::string>(&item.data);
                if (!s) { err.message = "schema_path item not string"; return false; }
                if (!encodeString(*s, out, err)) return false;
            }
            return true;
        }
        case FieldType::TYPE_SPEC: {
            auto v = std::get_if<TypeSpec>(&value.data);
            if (!v) { err.message = "expected TYPE_SPEC"; return false; }
            writeLE<uint16_t>(v->type_opcode, out);
            return encodeBytes(v->type_payload, out, err);
        }
        case FieldType::EXPR:
        case FieldType::STMT: {
            auto v = std::get_if<Value::InstrPtr>(&value.data);
            if (!v || !*v) { err.message = "expected instruction"; return false; }
            Buffer tmp;
            encodeInstruction(**v, tmp);
            out.insert(out.end(), tmp.begin(), tmp.end());
            return true;
        }
        case FieldType::EXPR_LIST:
        case FieldType::STMT_LIST: {
            auto v = std::get_if<Value::List>(&value.data);
            if (!v) { err.message = "expected list"; return false; }
            encodeVaruint(static_cast<uint64_t>(v->size()), out);
            for (const auto& item : *v) {
                auto instr = std::get_if<Value::InstrPtr>(&item.data);
                if (!instr || !*instr) { err.message = "list item not instruction"; return false; }
                Buffer tmp;
                encodeInstruction(**instr, tmp);
                out.insert(out.end(), tmp.begin(), tmp.end());
            }
            return true;
        }
        case FieldType::LIST: {
            auto v = std::get_if<Value::List>(&value.data);
            if (!v) { err.message = "expected list"; return false; }
            encodeVaruint(static_cast<uint64_t>(v->size()), out);
            FieldDef inner{field.name, FieldType::SCHEMA, field.ref};
            for (const auto& item : *v) {
                if (!encodeValue(inner, item, out, err)) return false;
            }
            return true;
        }
        case FieldType::OPT: {
            if (value.isNull()) {
                out.push_back(0);
                return true;
            }
            out.push_back(1);
            FieldDef inner{field.name, FieldType::SCHEMA, field.ref};
            return encodeValue(inner, value, out, err);
        }
        case FieldType::SCHEMA: {
            if (field.ref == "expr") {
                FieldDef f{field.name, FieldType::EXPR, {}};
                return encodeValue(f, value, out, err);
            }
            if (field.ref == "stmt") {
                FieldDef f{field.name, FieldType::STMT, {}};
                return encodeValue(f, value, out, err);
            }
            if (field.ref == "expr_list") {
                FieldDef f{field.name, FieldType::EXPR_LIST, {}};
                return encodeValue(f, value, out, err);
            }
            if (field.ref == "stmt_list") {
                FieldDef f{field.name, FieldType::STMT_LIST, {}};
                return encodeValue(f, value, out, err);
            }
            if (field.ref.rfind("list<", 0) == 0 && field.ref.back() == '>') {
                std::string inner_ref = field.ref.substr(5, field.ref.size() - 6);
                FieldDef f{field.name, FieldType::LIST, inner_ref};
                return encodeValue(f, value, out, err);
            }
            if (field.ref == "schema_path") {
                FieldDef f{field.name, FieldType::SCHEMA_PATH, {}};
                return encodeValue(f, value, out, err);
            }
            if (field.ref == "ident") {
                FieldDef f{field.name, FieldType::IDENT, {}};
                return encodeValue(f, value, out, err);
            }
            if (field.ref == "string") {
                FieldDef f{field.name, FieldType::STRING, {}};
                return encodeValue(f, value, out, err);
            }
            if (field.ref == "bytes") {
                FieldDef f{field.name, FieldType::BYTES, {}};
                return encodeValue(f, value, out, err);
            }
            if (field.ref == "u8") { FieldDef f{field.name, FieldType::U8, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "u16") { FieldDef f{field.name, FieldType::U16, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "u32") { FieldDef f{field.name, FieldType::U32, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "u64") { FieldDef f{field.name, FieldType::U64, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "i8") { FieldDef f{field.name, FieldType::I8, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "i16") { FieldDef f{field.name, FieldType::I16, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "i32") { FieldDef f{field.name, FieldType::I32, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "i64") { FieldDef f{field.name, FieldType::I64, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "u128") { FieldDef f{field.name, FieldType::U128, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "uuid") { FieldDef f{field.name, FieldType::UUID, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "f32") { FieldDef f{field.name, FieldType::F32, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "f64") { FieldDef f{field.name, FieldType::F64, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "bool") { FieldDef f{field.name, FieldType::BOOL, {}}; return encodeValue(f, value, out, err); }
            if (field.ref == "varuint") { FieldDef f{field.name, FieldType::VARUINT, {}}; return encodeValue(f, value, out, err); }
            const SchemaDef* schema = lookupSchema(field.ref);
            if (!schema) { err.message = "unknown schema: " + field.ref; return false; }
            return encodePayloadBySchema(*schema, value, out, err);
        }
    }
    err.message = "unsupported field type";
    return false;
}

bool decodeValue(const FieldDef& field, const uint8_t* data, size_t size, size_t& offset, Value& out, DecodeError& err) {
    switch (field.type) {
        case FieldType::U8: {
            if (offset + 1 > size) { err.message = "u8 out of range"; return false; }
            out = Value(static_cast<uint64_t>(data[offset++]));
            return true;
        }
        case FieldType::U16: {
            uint16_t v; if (!readLE<uint16_t>(data, size, offset, v)) { err.message = "u16"; return false; }
            out = Value(static_cast<uint64_t>(v));
            return true;
        }
        case FieldType::U32: {
            uint32_t v; if (!readLE<uint32_t>(data, size, offset, v)) { err.message = "u32"; return false; }
            out = Value(static_cast<uint64_t>(v));
            return true;
        }
        case FieldType::U64: {
            uint64_t v; if (!readLE<uint64_t>(data, size, offset, v)) { err.message = "u64"; return false; }
            out = Value(v);
            return true;
        }
        case FieldType::I8: {
            if (offset + 1 > size) { err.message = "i8"; return false; }
            int8_t v = static_cast<int8_t>(data[offset++]);
            out = Value(static_cast<int64_t>(v));
            return true;
        }
        case FieldType::I16: {
            int16_t v; if (!readLE<int16_t>(data, size, offset, v)) { err.message = "i16"; return false; }
            out = Value(static_cast<int64_t>(v));
            return true;
        }
        case FieldType::I32: {
            int32_t v; if (!readLE<int32_t>(data, size, offset, v)) { err.message = "i32"; return false; }
            out = Value(static_cast<int64_t>(v));
            return true;
        }
        case FieldType::I64: {
            int64_t v; if (!readLE<int64_t>(data, size, offset, v)) { err.message = "i64"; return false; }
            out = Value(v);
            return true;
        }
        case FieldType::U128:
        case FieldType::UUID: {
            if (offset + 16 > size) { err.message = "u128"; return false; }
            Value::Bytes b(data + offset, data + offset + 16);
            offset += 16;
            out = Value(std::move(b));
            return true;
        }
        case FieldType::F32: {
            uint32_t raw; if (!readLE<uint32_t>(data, size, offset, raw)) { err.message = "f32"; return false; }
            float f; std::memcpy(&f, &raw, sizeof(f));
            out = Value(static_cast<double>(f));
            return true;
        }
        case FieldType::F64: {
            uint64_t raw; if (!readLE<uint64_t>(data, size, offset, raw)) { err.message = "f64"; return false; }
            double d; std::memcpy(&d, &raw, sizeof(d));
            out = Value(d);
            return true;
        }
        case FieldType::BOOL: {
            if (offset + 1 > size) { err.message = "bool"; return false; }
            out = Value(data[offset++] != 0);
            return true;
        }
        case FieldType::VARUINT: {
            uint64_t v; if (!decodeVaruint(data, size, offset, v)) { err.message = "varuint"; return false; }
            out = Value(v);
            return true;
        }
        case FieldType::STRING:
        case FieldType::IDENT: {
            std::string s; if (!decodeString(data, size, offset, s, err)) return false;
            out = Value(std::move(s));
            return true;
        }
        case FieldType::BYTES: {
            Value::Bytes b; if (!decodeBytes(data, size, offset, b, err)) return false;
            out = Value(std::move(b));
            return true;
        }
        case FieldType::SCHEMA_PATH: {
            uint64_t count; if (!decodeVaruint(data, size, offset, count)) { err.message = "schema_path"; return false; }
            Value::List list; list.reserve(static_cast<size_t>(count));
            for (size_t i = 0; i < count; ++i) {
                std::string s; if (!decodeString(data, size, offset, s, err)) return false;
                list.emplace_back(Value(std::move(s)));
            }
            out = Value(std::move(list));
            return true;
        }
        case FieldType::TYPE_SPEC: {
            uint16_t op; if (!readLE<uint16_t>(data, size, offset, op)) { err.message = "type_opcode"; return false; }
            Value::Bytes b; if (!decodeBytes(data, size, offset, b, err)) return false;
            out = Value(TypeSpec{op, std::move(b)});
            return true;
        }
        case FieldType::EXPR:
        case FieldType::STMT: {
            Instruction inst; if (!decodeInstruction(data, size, offset, inst, err)) return false;
            out = Value(std::make_shared<Instruction>(std::move(inst)));
            return true;
        }
        case FieldType::EXPR_LIST:
        case FieldType::STMT_LIST: {
            uint64_t count; if (!decodeVaruint(data, size, offset, count)) { err.message = "list"; return false; }
            Value::List list; list.reserve(static_cast<size_t>(count));
            for (size_t i = 0; i < count; ++i) {
                Instruction inst; if (!decodeInstruction(data, size, offset, inst, err)) return false;
                list.emplace_back(Value(std::make_shared<Instruction>(std::move(inst))));
            }
            out = Value(std::move(list));
            return true;
        }
        case FieldType::LIST: {
            uint64_t count; if (!decodeVaruint(data, size, offset, count)) { err.message = "list"; return false; }
            Value::List list; list.reserve(static_cast<size_t>(count));
            FieldDef inner{field.name, FieldType::SCHEMA, field.ref};
            for (size_t i = 0; i < count; ++i) {
                Value item; if (!decodeValue(inner, data, size, offset, item, err)) return false;
                list.emplace_back(std::move(item));
            }
            out = Value(std::move(list));
            return true;
        }
        case FieldType::OPT: {
            if (offset + 1 > size) { err.message = "opt"; return false; }
            uint8_t present = data[offset++];
            if (!present) { out = Value(); return true; }
            FieldDef inner{field.name, FieldType::SCHEMA, field.ref};
            return decodeValue(inner, data, size, offset, out, err);
        }
        case FieldType::SCHEMA: {
            if (field.ref == "expr") {
                FieldDef f{field.name, FieldType::EXPR, {}};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref == "stmt") {
                FieldDef f{field.name, FieldType::STMT, {}};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref == "expr_list") {
                FieldDef f{field.name, FieldType::EXPR_LIST, {}};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref == "stmt_list") {
                FieldDef f{field.name, FieldType::STMT_LIST, {}};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref.rfind("list<", 0) == 0 && field.ref.back() == '>') {
                std::string inner_ref = field.ref.substr(5, field.ref.size() - 6);
                FieldDef f{field.name, FieldType::LIST, inner_ref};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref == "schema_path") {
                FieldDef f{field.name, FieldType::SCHEMA_PATH, {}};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref == "ident") {
                FieldDef f{field.name, FieldType::IDENT, {}};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref == "string") {
                FieldDef f{field.name, FieldType::STRING, {}};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref == "bytes") {
                FieldDef f{field.name, FieldType::BYTES, {}};
                return decodeValue(f, data, size, offset, out, err);
            }
            if (field.ref == "u8") { FieldDef f{field.name, FieldType::U8, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "u16") { FieldDef f{field.name, FieldType::U16, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "u32") { FieldDef f{field.name, FieldType::U32, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "u64") { FieldDef f{field.name, FieldType::U64, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "i8") { FieldDef f{field.name, FieldType::I8, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "i16") { FieldDef f{field.name, FieldType::I16, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "i32") { FieldDef f{field.name, FieldType::I32, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "i64") { FieldDef f{field.name, FieldType::I64, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "u128") { FieldDef f{field.name, FieldType::U128, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "uuid") { FieldDef f{field.name, FieldType::UUID, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "f32") { FieldDef f{field.name, FieldType::F32, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "f64") { FieldDef f{field.name, FieldType::F64, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "bool") { FieldDef f{field.name, FieldType::BOOL, {}}; return decodeValue(f, data, size, offset, out, err); }
            if (field.ref == "varuint") { FieldDef f{field.name, FieldType::VARUINT, {}}; return decodeValue(f, data, size, offset, out, err); }
            const SchemaDef* schema = lookupSchema(field.ref);
            if (!schema) { err.message = "unknown schema: " + field.ref; return false; }
            return decodePayloadBySchema(*schema, data, size, offset, out, err);
        }
    }
    err.message = "unsupported field type";
    return false;
}

}  // namespace scratchbird::sblr::v3
