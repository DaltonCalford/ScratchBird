#include "scratchbird/sblr/v3_payloads.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>

namespace scratchbird::sblr::v3 {

extern const std::unordered_map<std::string, std::string> kOpcodeSchemaMapGenerated;

namespace {
    void writeLE32(uint32_t v, Buffer &out) {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    }

    void writeLE64(uint64_t v, Buffer &out) {
        for (int i = 0; i < 8; ++i) {
            out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }

    void writeLEDouble(double d, Buffer &out) {
        static_assert(sizeof(double) == sizeof(uint64_t), "double size");
        uint64_t v;
        std::memcpy(&v, &d, sizeof(v));
        writeLE64(v, out);
    }

    void encodeVaruint(uint64_t v, Buffer &out) {
        while (v >= 0x80) {
            out.push_back(static_cast<uint8_t>(v | 0x80));
            v >>= 7;
        }
        out.push_back(static_cast<uint8_t>(v));
    }

    bool encodeLiteralPayload(uint16_t opcode, const Value &payload, Buffer &out, DecodeError &err) {
        const auto *obj = std::get_if<Value::Object>(&payload.data);
        if (!obj) {
            return false;
        }
        auto it = obj->find("value");
        if (it == obj->end()) {
            return false;
        }
        const auto &val = it->second.data;
        const char *name_c = opcodeName(opcode);
        std::string name = name_c ? name_c : "";

        if (name == "SBLR3_LITERAL_NULL") {
            return true;
        }
        if (name == "SBLR3_LITERAL_BOOLEAN") {
            auto b = std::get_if<bool>(&val);
            if (!b) { err.message = "literal bool"; return false; }
            out.push_back(*b ? 1 : 0);
            return true;
        }
        if (name == "SBLR3_LITERAL_INT32") {
            auto i = std::get_if<int64_t>(&val);
            if (!i) { err.message = "literal int32"; return false; }
            writeLE32(static_cast<uint32_t>(*i), out);
            return true;
        }
        if (name == "SBLR3_LITERAL_INT64") {
            auto i = std::get_if<int64_t>(&val);
            if (!i) { err.message = "literal int64"; return false; }
            writeLE64(static_cast<uint64_t>(*i), out);
            return true;
        }
        if (name == "SBLR3_LITERAL_DOUBLE") {
            auto d = std::get_if<double>(&val);
            if (!d) { err.message = "literal double"; return false; }
            writeLEDouble(*d, out);
            return true;
        }
        if (name == "SBLR3_LITERAL_STRING" || name == "SBLR3_LITERAL_JSON" ||
            name == "SBLR3_LITERAL_XML" || name == "SBLR3_LITERAL_DECIMAL") {
            auto s = std::get_if<std::string>(&val);
            if (!s) { err.message = "literal string"; return false; }
            encodeVaruint(static_cast<uint64_t>(s->size()), out);
            out.insert(out.end(), s->begin(), s->end());
            return true;
        }
        if (name == "SBLR3_LITERAL_BINARY") {
            auto b = std::get_if<Value::Bytes>(&val);
            if (!b) { err.message = "literal binary"; return false; }
            encodeVaruint(static_cast<uint64_t>(b->size()), out);
            out.insert(out.end(), b->begin(), b->end());
            return true;
        }
        if (name == "SBLR3_LITERAL_UUID") {
            auto b = std::get_if<Value::Bytes>(&val);
            if (!b || b->size() != 16) { err.message = "literal uuid"; return false; }
            out.insert(out.end(), b->begin(), b->end());
            return true;
        }
        if (name == "SBLR3_LITERAL_DATE") {
            auto i = std::get_if<int64_t>(&val);
            if (!i) { err.message = "literal date"; return false; }
            writeLE32(static_cast<uint32_t>(*i), out);
            return true;
        }
        if (name == "SBLR3_LITERAL_TIME" || name == "SBLR3_LITERAL_TIMESTAMP") {
            auto i = std::get_if<int64_t>(&val);
            if (!i) { err.message = "literal time"; return false; }
            writeLE64(static_cast<uint64_t>(*i), out);
            return true;
        }
        return false;
    }
} // namespace


static const std::unordered_map<std::string, std::string> kExprUnary = {
    {"SBLR3_EXPR_NOT", "SCHEMA_EXPR_UNARY"},
    {"SBLR3_BIT_NOT", "SCHEMA_EXPR_UNARY"},
    {"SBLR3_EXPR_IS_NULL", "SCHEMA_EXPR_UNARY"},
};

static const std::unordered_map<std::string, std::string> kExprBinary = {
    {"SBLR3_EXPR_ADD", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_SUBTRACT", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_MULTIPLY", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_DIVIDE", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_DIV_INT", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_MODULO", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_EQ", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_NE", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_LT", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_LE", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_GT", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_GE", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_NULL_SAFE_EQ", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_AND", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_EXPR_OR", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_BIT_AND", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_BIT_OR", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_BIT_XOR", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_BIT_SHIFT_LEFT", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_BIT_SHIFT_RIGHT", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_BIT_SHIFT_RIGHT_LOGICAL", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_REGEX_MATCH", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_REGEX_MATCH_CI", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_REGEX_NOT_MATCH", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_REGEX_NOT_MATCH_CI", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_JSON_EXTRACT", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_JSON_DOUBLE_ARROW", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_JSON_HASH_ARROW", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_JSON_HASH_DOUBLE_ARROW", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_ARRAY_CONTAINS", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_ARRAY_CONTAINED_BY", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_ARRAY_OVERLAP", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_PRED_CONTAINING", "SCHEMA_EXPR_BINARY"},
    {"SBLR3_PRED_STARTING_WITH", "SCHEMA_EXPR_BINARY"},
};

const SchemaDef* schemaForOpcode(uint16_t opcode) {
    const char* name_c = opcodeName(opcode);
    if (!name_c) return nullptr;
    std::string name(name_c);

    if (name == "SBLR3_VERSION" || name == "SBLR3_END" || name == "SBLR3_EXTENDED_OPCODE") {
        return nullptr;
    }

    if (name.rfind("SBLR3_LITERAL_", 0) == 0) {
        std::string schema = "SCHEMA_LITERAL_" + name.substr(std::string("SBLR3_LITERAL_").size());
        return lookupSchema(schema);
    }

    if (name == "SBLR3_EXPR_FUNCTION_CALL" || name.rfind("SBLR3_FUNC_", 0) == 0) {
        return lookupSchema("SCHEMA_FUNC_CALL");
    }
    if (name == "SBLR3_JSON_OBJECT" || name == "SBLR3_JSON_ARRAY" ||
        name == "SBLR3_JSON_SET" || name == "SBLR3_JSON_INSERT" ||
        name == "SBLR3_JSON_REMOVE") {
        return lookupSchema("SCHEMA_FUNC_CALL");
    }

    if (name.rfind("SBLR3_AGG_", 0) == 0) {
        return lookupSchema("SCHEMA_AGG_CALL");
    }
    if (name == "SBLR3_XMLAGG") {
        return lookupSchema("SCHEMA_AGG_CALL");
    }

    if (name.rfind("SBLR3_WIN_", 0) == 0) {
        return lookupSchema("SCHEMA_WINDOW_CALL");
    }

    if (name == "SBLR3_TO_TSVECTOR" || name == "SBLR3_PLAINTO_TSQUERY" ||
        name == "SBLR3_TO_TSQUERY" || name == "SBLR3_TSMATCH" ||
        name == "SBLR3_TS_RANK") {
        return lookupSchema("SCHEMA_FUNC_CALL");
    }

    if (auto it = kExprUnary.find(name); it != kExprUnary.end()) {
        return lookupSchema(it->second);
    }
    if (auto it = kExprBinary.find(name); it != kExprBinary.end()) {
        return lookupSchema(it->second);
    }

    if (name == "SBLR3_EXPR_CAST") return lookupSchema("SCHEMA_EXPR_CAST");
    if (name == "SBLR3_COALESCE" || name == "SBLR3_NULLIF") {
        return lookupSchema("SCHEMA_FUNC_CALL");
    }
    if (name == "SBLR3_CASE_WHEN") return lookupSchema("SCHEMA_EXPR_CASE");
    if (name == "SBLR3_IN_LIST" || name == "SBLR3_SUBQUERY_IN" || name == "SBLR3_SUBQUERY_NOT_IN") {
        return lookupSchema("SCHEMA_EXPR_IN");
    }
    if (name == "SBLR3_EXPR_LIKE" || name == "SBLR3_EXPR_ILIKE" || name == "SBLR3_LIKE_ESCAPE" || name == "SBLR3_ILIKE_ESCAPE") {
        return lookupSchema("SCHEMA_EXPR_LIKE");
    }
    if (name == "SBLR3_SUBQUERY_EXISTS") return lookupSchema("SCHEMA_EXPR_EXISTS");
    if (name == "SBLR3_SUBQUERY_SCALAR") return lookupSchema("SCHEMA_EXPR_SUBQUERY");
    if (name == "SBLR3_EXTRACT" || name == "SBLR3_ALTER_ELEMENT") return lookupSchema("SCHEMA_FUNC_CALL");
    if (name == "SBLR3_RENAME_OBJECT" || name == "SBLR3_MOVE_OBJECT") return lookupSchema("SCHEMA_DDL_ALTER_RENAME");

    if (name == "SBLR3_SET" || name == "SBLR3_SHOW" || name == "SBLR3_RESET" ||
        name.rfind("SBLR3_SET_", 0) == 0 || name.rfind("SBLR3_SHOW_", 0) == 0 ||
        name.rfind("SBLR3_RESET_", 0) == 0) {
        return lookupSchema("SCHEMA_SET_SHOW_RESET");
    }
    if (name == "SBLR3_EXPLAIN_PLAN") return lookupSchema("SCHEMA_EXPLAIN");

    if (name == "SBLR3_CREATE_FUNCTION_STMT") return lookupSchema("SCHEMA_DDL_CREATE_FUNCTION");
    if (name == "SBLR3_CREATE_PROCEDURE_STMT") return lookupSchema("SCHEMA_DDL_CREATE_PROCEDURE");
    if (name == "SBLR3_CREATE_EXCEPTION_STMT") return lookupSchema("SCHEMA_DDL_CREATE_EXCEPTION");
    if (name == "SBLR3_CREATE_USER") return lookupSchema("SCHEMA_DDL_CREATE_USER");
    if (name == "SBLR3_CREATE_ROLE") return lookupSchema("SCHEMA_DDL_CREATE_ROLE");
    if (name == "SBLR3_CREATE_GROUP") return lookupSchema("SCHEMA_DDL_CREATE_GROUP");
    if (name == "SBLR3_DROP_FUNCTION_STMT" || name == "SBLR3_DROP_PROCEDURE_STMT" ||
        name == "SBLR3_DROP_TRIGGER" || name == "SBLR3_DROP_PACKAGE_STMT" ||
        name == "SBLR3_DROP_ROLE" || name == "SBLR3_DROP_GROUP" ||
        name == "SBLR3_DROP_USER" || name == "SBLR3_DROP_EXCEPTION_STMT" ||
        name == "SBLR3_DROP_DOMAIN" || name == "SBLR3_DROP_TYPE" ||
        name == "SBLR3_DROP_TABLE" || name == "SBLR3_DROP_INDEX" ||
        name == "SBLR3_DROP_VIEW" || name == "SBLR3_DROP_SEQUENCE" ||
        name == "SBLR3_DROP_SCHEMA" || name == "SBLR3_DROP_TABLESPACE" ||
        name == "SBLR3_DROP_DATABASE" || name == "SBLR3_DROP_POLICY" ||
        name == "SBLR3_DROP_FOREIGN_SERVER" || name == "SBLR3_DROP_FOREIGN_TABLE" ||
        name == "SBLR3_DROP_USER_MAPPING" || name == "SBLR3_DROP_SYNONYM" ||
        name == "SBLR3_DROP_UDR" || name == "SBLR3_DROP_JOB") {
        return lookupSchema("SCHEMA_DDL_DROP");
    }
    if (name == "SBLR3_TRUNCATE_TABLE") return lookupSchema("SCHEMA_DDL_TRUNCATE");
    if (name == "SBLR3_COMMENT") return lookupSchema("SCHEMA_DDL_COMMENT");

    if (auto it = kOpcodeSchemaMapGenerated.find(name); it != kOpcodeSchemaMapGenerated.end()) {
        return lookupSchema(it->second);
    }

    // ARRAY/TEXTSEARCH/SPATIAL default to func call
    if (name.find("ARRAY") != std::string::npos || name.find("TSVECTOR") != std::string::npos ||
        name.find("TSQUERY") != std::string::npos || name.find("SPATIAL") != std::string::npos) {
        return lookupSchema("SCHEMA_FUNC_CALL");
    }

    return nullptr;
}

bool encodeInstructionWithSchema(const Instruction& inst, Buffer& out, DecodeError& err) {
    Buffer payload;
    const SchemaDef* schema = schemaForOpcode(inst.opcode);

    if (schema) {
        if (!encodePayloadBySchema(*schema, inst.payload, payload, err)) return false;
    } else {
        if (auto bytes = std::get_if<Value::Bytes>(&inst.payload.data)) {
            payload = *bytes;
        } else if (std::holds_alternative<Value::Object>(inst.payload.data)) {
            if (!encodeLiteralPayload(inst.opcode, inst.payload, payload, err)) {
                const char* name_c = opcodeName(inst.opcode);
                std::string name = name_c ? name_c : "UNKNOWN";
                err.message = "missing schema for opcode payload: " + name;
                return false;
            }
        }
    }

    // Header
    auto writeLE16 = [&](uint16_t v) { out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF); };
    auto writeLE32 = [&](uint32_t v) {
        out.push_back(v & 0xFF);
        out.push_back((v >> 8) & 0xFF);
        out.push_back((v >> 16) & 0xFF);
        out.push_back((v >> 24) & 0xFF);
    };

    writeLE16(inst.opcode);
    writeLE16(inst.flags);
    writeLE32(static_cast<uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return true;
}

bool decodeInstructionWithSchema(const uint8_t* data, size_t size, size_t& offset, Instruction& out, DecodeError& err) {
    if (offset + 8 > size) { err.message = "instruction header"; return false; }
    uint16_t opcode = data[offset] | (data[offset + 1] << 8);
    uint16_t flags = data[offset + 2] | (data[offset + 3] << 8);
    uint32_t payload_len = data[offset + 4] | (data[offset + 5] << 8) | (data[offset + 6] << 16) | (data[offset + 7] << 24);
    offset += 8;
    if (offset + payload_len > size) { err.message = "payload bounds"; return false; }
    size_t payload_off = offset;

    out.opcode = opcode;
    out.flags = flags;

    const SchemaDef* schema = schemaForOpcode(opcode);
    if (schema) {
        Value payload;
        size_t tmp_off = payload_off;
        if (!decodePayloadBySchema(*schema, data, size, tmp_off, payload, err)) return false;
        if (tmp_off != payload_off + payload_len) {
            err.message = "payload length mismatch";
            return false;
        }
        out.payload = std::move(payload);
    } else {
        out.payload = Value(Value::Bytes{data + payload_off, data + payload_off + payload_len});
    }

    offset = payload_off + payload_len;
    return true;
}

}  // namespace scratchbird::sblr::v3
