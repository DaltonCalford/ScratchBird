#include "scratchbird/sblr/v3_container.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string_view>

#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_opcode_identity.h"

namespace scratchbird::sblr::v3 {

namespace {

enum class RetainedValueTag : uint8_t {
    Null = 0,
    Bool = 1,
    I64 = 2,
    U64 = 3,
    String = 4,
    List = 5,
    Object = 6
};

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

bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() &&
           text.substr(text.size() - suffix.size()) == suffix;
}

bool encodeRetainedSymbolValue(const Value& value,
                               std::vector<uint8_t>& out,
                               std::string& err) {
    DecodeError derr;
    if (std::holds_alternative<std::monostate>(value.data)) {
        out.push_back(static_cast<uint8_t>(RetainedValueTag::Null));
        return true;
    }
    if (const auto* boolean = std::get_if<bool>(&value.data)) {
        out.push_back(static_cast<uint8_t>(RetainedValueTag::Bool));
        out.push_back(*boolean ? 1 : 0);
        return true;
    }
    if (const auto* i64 = std::get_if<int64_t>(&value.data)) {
        out.push_back(static_cast<uint8_t>(RetainedValueTag::I64));
        writeLE64(static_cast<uint64_t>(*i64), out);
        return true;
    }
    if (const auto* u64 = std::get_if<uint64_t>(&value.data)) {
        out.push_back(static_cast<uint8_t>(RetainedValueTag::U64));
        writeLE64(*u64, out);
        return true;
    }
    if (const auto* text = std::get_if<std::string>(&value.data)) {
        out.push_back(static_cast<uint8_t>(RetainedValueTag::String));
        if (!encodeString(*text, out, derr)) {
            err = derr.message;
            return false;
        }
        return true;
    }
    if (const auto* list = std::get_if<Value::List>(&value.data)) {
        out.push_back(static_cast<uint8_t>(RetainedValueTag::List));
        encodeVaruint(list->size(), out);
        for (const auto& entry : *list) {
            if (!encodeRetainedSymbolValue(entry, out, err)) {
                return false;
            }
        }
        return true;
    }
    if (const auto* object = std::get_if<Value::Object>(&value.data)) {
        out.push_back(static_cast<uint8_t>(RetainedValueTag::Object));
        encodeVaruint(object->size(), out);
        for (const auto& [key, entry] : *object) {
            if (!encodeString(key, out, derr)) {
                err = derr.message;
                return false;
            }
            if (!encodeRetainedSymbolValue(entry, out, err)) {
                return false;
            }
        }
        return true;
    }

    err = "unsupported retained symbol value type";
    return false;
}

bool decodeRetainedSymbolValue(const uint8_t* data,
                               size_t size,
                               size_t& off,
                               Value& out,
                               std::string& err) {
    if (off >= size) {
        err = "retained value tag";
        return false;
    }
    const auto tag = static_cast<RetainedValueTag>(data[off++]);
    DecodeError derr;
    switch (tag) {
        case RetainedValueTag::Null:
            out = Value();
            return true;
        case RetainedValueTag::Bool:
            if (off >= size) {
                err = "retained bool";
                return false;
            }
            out = Value(data[off++] != 0);
            return true;
        case RetainedValueTag::I64: {
            uint64_t raw = 0;
            if (!readLE64(data, size, off, raw)) {
                err = "retained i64";
                return false;
            }
            out = Value(static_cast<int64_t>(raw));
            return true;
        }
        case RetainedValueTag::U64: {
            uint64_t raw = 0;
            if (!readLE64(data, size, off, raw)) {
                err = "retained u64";
                return false;
            }
            out = Value(raw);
            return true;
        }
        case RetainedValueTag::String: {
            std::string text;
            if (!decodeString(data, size, off, text, derr)) {
                err = derr.message;
                return false;
            }
            out = Value(std::move(text));
            return true;
        }
        case RetainedValueTag::List: {
            uint64_t count = 0;
            if (!decodeVaruint(data, size, off, count)) {
                err = "retained list count";
                return false;
            }
            Value::List list;
            list.reserve(static_cast<size_t>(count));
            for (size_t i = 0; i < count; ++i) {
                Value entry;
                if (!decodeRetainedSymbolValue(data, size, off, entry, err)) {
                    return false;
                }
                list.push_back(std::move(entry));
            }
            out = Value(std::move(list));
            return true;
        }
        case RetainedValueTag::Object: {
            uint64_t count = 0;
            if (!decodeVaruint(data, size, off, count)) {
                err = "retained object count";
                return false;
            }
            Value::Object object;
            for (size_t i = 0; i < count; ++i) {
                std::string key;
                if (!decodeString(data, size, off, key, derr)) {
                    err = derr.message;
                    return false;
                }
                Value entry;
                if (!decodeRetainedSymbolValue(data, size, off, entry, err)) {
                    return false;
                }
                object.emplace(std::move(key), std::move(entry));
            }
            out = Value(std::move(object));
            return true;
        }
    }

    err = "unknown retained value tag";
    return false;
}

bool encodeRetainedSymbolPayload(const Value::Object& payload,
                                 std::vector<uint8_t>& out,
                                 std::string& err) {
    return encodeRetainedSymbolValue(Value(payload), out, err);
}

bool decodeRetainedSymbolPayload(const uint8_t* data,
                                 size_t size,
                                 size_t& off,
                                 Value::Object& out,
                                 std::string& err) {
    Value decoded;
    if (!decodeRetainedSymbolValue(data, size, off, decoded, err)) {
        return false;
    }
    auto* object = std::get_if<Value::Object>(&decoded.data);
    if (object == nullptr) {
        err = "retained symbol payload root must be object";
        return false;
    }
    out = std::move(*object);
    return true;
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

class RetainedSymbolBuilder {
public:
    Value::Object build(const Instruction& root) {
        root_opcode_symbol_ = canonicalOpcodeSymbolForOpcode(root.opcode);
        addScope(1, 0, "statement_root", "root", 0);
        next_scope_id_ = 2;
        walk(root.payload, 1, "root");

        Value::List source_order_registry;
        for (const auto& [scope_id, symbols] : source_order_registry_) {
            Value::Object entry;
            entry["scope_id"] = Value(scope_id);
            entry["symbol_ids"] = Value(symbols);
            source_order_registry.push_back(Value(std::move(entry)));
        }

        Value::Object payload;
        payload["format_version"] = Value(uint64_t(1));
        payload["root_opcode_symbol"] =
            Value(root_opcode_symbol_.empty() ? std::string() : root_opcode_symbol_);
        payload["symbol_registry"] = Value(std::move(symbol_registry_));
        payload["scope_registry"] = Value(std::move(scope_registry_));
        payload["scope_parent_map"] = Value(std::move(scope_parent_map_));
        payload["display_name_registry"] = Value(std::move(display_name_registry_));
        payload["parameter_display_registry"] = Value(std::move(parameter_display_registry_));
        payload["output_label_registry"] = Value(std::move(output_label_registry_));
        payload["placeholder_binding_registry"] =
            Value(std::move(placeholder_binding_registry_));
        payload["source_order_registry"] = Value(std::move(source_order_registry));
        return payload;
    }

private:
    uint64_t next_symbol_id_ = 1;
    uint64_t next_scope_id_ = 1;
    uint64_t next_display_name_id_ = 1;
    std::string root_opcode_symbol_;
    Value::List symbol_registry_;
    Value::List scope_registry_;
    Value::List scope_parent_map_;
    Value::List display_name_registry_;
    Value::List parameter_display_registry_;
    Value::List output_label_registry_;
    Value::List placeholder_binding_registry_;
    std::map<std::pair<std::string, bool>, uint64_t> display_name_ids_;
    std::map<uint64_t, Value::List> source_order_registry_;

    static std::string childPath(const std::string& parent, std::string_view segment) {
        std::string path = parent;
        if (!path.empty()) {
            path.push_back('.');
        }
        path.append(segment);
        return path;
    }

    static std::string listItemPath(const std::string& parent, size_t ordinal) {
        std::string path = parent;
        path.push_back('[');
        path.append(std::to_string(ordinal));
        path.push_back(']');
        return path;
    }

    static std::string symbolClassForField(std::string_view field_name,
                                           const std::string& path) {
        if (field_name == "alias" || endsWith(field_name, "_alias")) {
            return "relation_alias_symbol";
        }
        if (field_name == "cte_name" ||
            (field_name == "name" && path.find("cte") != std::string::npos)) {
            return "cte_symbol";
        }
        if (field_name == "cursor_name") {
            return "cursor_symbol";
        }
        if (field_name == "loop_label" || field_name == "block_label" ||
            field_name == "label") {
            return "block_label_symbol";
        }
        if (field_name == "exception_name" || field_name == "condition_name") {
            return "exception_symbol";
        }
        if (field_name == "event_name") {
            return "event_symbol";
        }
        if (field_name == "parameter_name" || field_name == "param_name" ||
            ((field_name == "name") &&
             (path.find("arguments") != std::string::npos ||
              path.find("parameters") != std::string::npos))) {
            return "parameter_symbol";
        }
        if (field_name == "name" &&
            (path.find("declare_variable") != std::string::npos ||
             path.find("variables") != std::string::npos)) {
            return "variable_symbol";
        }
        if (field_name == "key" && path.find("options") != std::string::npos) {
            return "option_key_symbol";
        }
        if (field_name == "savepoint_name") {
            return "transaction_local_control_symbol";
        }
        if (field_name == "user_name" || field_name == "role_name" ||
            field_name == "group_name" || field_name == "grantee_name") {
            return "security_principal_symbol";
        }
        if (field_name == "procedure_name") {
            return "procedure_symbol";
        }
        return {};
    }

    static std::string originClassForSymbol(std::string_view symbol_class) {
        if (symbol_class == "security_principal_symbol" ||
            symbol_class == "procedure_symbol") {
            return "durable_catalog_object";
        }
        if (symbol_class == "transaction_local_control_symbol") {
            return "transaction_local_control_symbol";
        }
        return "local_user_authored_symbol";
    }

    void addScope(uint64_t scope_id,
                  uint64_t parent_scope_id,
                  std::string_view scope_class,
                  const std::string& scope_path,
                  uint64_t ordinal) {
        Value::Object scope;
        scope["scope_id"] = Value(scope_id);
        scope["scope_class"] = Value(std::string(scope_class));
        scope["scope_path"] = Value(scope_path);
        scope["ordinal"] = Value(ordinal);
        scope_registry_.push_back(Value(std::move(scope)));
        if (parent_scope_id != 0) {
            Value::Object parent_map;
            parent_map["scope_id"] = Value(scope_id);
            parent_map["parent_scope_id"] = Value(parent_scope_id);
            scope_parent_map_.push_back(Value(std::move(parent_map)));
        }
    }

    uint64_t createScope(uint64_t parent_scope_id,
                         std::string_view scope_class,
                         const std::string& scope_path,
                         uint64_t ordinal) {
        const uint64_t scope_id = next_scope_id_++;
        addScope(scope_id, parent_scope_id, scope_class, scope_path, ordinal);
        return scope_id;
    }

    uint64_t displayNameId(const std::string& display_name, bool quoted) {
        const auto key = std::make_pair(display_name, quoted);
        auto it = display_name_ids_.find(key);
        if (it != display_name_ids_.end()) {
            return it->second;
        }
        const uint64_t display_name_id = next_display_name_id_++;
        display_name_ids_.emplace(key, display_name_id);
        Value::Object entry;
        entry["display_name_id"] = Value(display_name_id);
        entry["display_name"] = Value(display_name);
        entry["quoted"] = Value(quoted);
        display_name_registry_.push_back(Value(std::move(entry)));
        return display_name_id;
    }

    void addSourceOrder(uint64_t scope_id, uint64_t symbol_id) {
        source_order_registry_[scope_id].push_back(Value(symbol_id));
    }

    void addSymbol(std::string_view symbol_class,
                   uint64_t scope_id,
                   const std::string& display_name,
                   uint64_t ordinal,
                   bool quoted,
                   std::optional<uint64_t> output_position = std::nullopt) {
        if (display_name.empty()) {
            return;
        }
        const uint64_t symbol_id = next_symbol_id_++;
        const uint64_t display_name_id = displayNameId(display_name, quoted);

        Value::Object symbol;
        symbol["symbol_id"] = Value(symbol_id);
        symbol["scope_id"] = Value(scope_id);
        symbol["symbol_class"] = Value(std::string(symbol_class));
        symbol["display_name_id"] = Value(display_name_id);
        symbol["ordinal"] = Value(ordinal);
        symbol["symbol_origin_class"] =
            Value(originClassForSymbol(symbol_class));
        symbol["user_supplied"] = Value(true);
        symbol["quoted"] = Value(quoted);
        symbol_registry_.push_back(Value(std::move(symbol)));
        addSourceOrder(scope_id, symbol_id);

        if (symbol_class == "parameter_symbol") {
            Value::Object parameter_display;
            parameter_display["symbol_id"] = Value(symbol_id);
            parameter_display["display_name_id"] = Value(display_name_id);
            parameter_display_registry_.push_back(Value(std::move(parameter_display)));
        }

        if (symbol_class == "output_label_symbol" && output_position.has_value()) {
            Value::Object output_label;
            output_label["symbol_id"] = Value(symbol_id);
            output_label["position"] = Value(*output_position);
            output_label_registry_.push_back(Value(std::move(output_label)));
        }
    }

    void recordStringField(std::string_view field_name,
                           const std::string& display_name,
                           uint64_t scope_id,
                           const std::string& path,
                           uint64_t ordinal) {
        const std::string symbol_class = symbolClassForField(field_name, path);
        if (symbol_class.empty()) {
            return;
        }
        addSymbol(symbol_class, scope_id, display_name, ordinal, false);
    }

    void walk(const Value& value, uint64_t scope_id, const std::string& path) {
        if (const auto* object = std::get_if<Value::Object>(&value.data)) {
            size_t ordinal = 0;
            for (const auto& [key, entry] : *object) {
                const std::string next_path = childPath(path, key);
                if (key == "select_aliases") {
                    if (const auto* aliases = std::get_if<Value::List>(&entry.data)) {
                        for (size_t i = 0; i < aliases->size(); ++i) {
                            if (const auto* alias =
                                    std::get_if<std::string>(&(*aliases)[i].data);
                                alias != nullptr && !alias->empty()) {
                                addSymbol("output_label_symbol",
                                          scope_id,
                                          *alias,
                                          static_cast<uint64_t>(i),
                                          false,
                                          static_cast<uint64_t>(i));
                            }
                        }
                    }
                    ++ordinal;
                    continue;
                }

                if (const auto* text = std::get_if<std::string>(&entry.data)) {
                    recordStringField(key, *text, scope_id, path, ordinal);
                    ++ordinal;
                    continue;
                }

                if (std::holds_alternative<Value::Object>(entry.data)) {
                    const uint64_t child_scope_id =
                        createScope(scope_id, "payload_object", next_path, ordinal);
                    walk(entry, child_scope_id, next_path);
                    ++ordinal;
                    continue;
                }

                if (const auto* list = std::get_if<Value::List>(&entry.data)) {
                    const uint64_t child_scope_id =
                        createScope(scope_id, "payload_list", next_path, ordinal);
                    for (size_t i = 0; i < list->size(); ++i) {
                        walk((*list)[i], child_scope_id, listItemPath(next_path, i));
                    }
                    ++ordinal;
                    continue;
                }

                if (const auto* instr = std::get_if<Value::InstrPtr>(&entry.data);
                    instr != nullptr && *instr) {
                    const uint64_t child_scope_id =
                        createScope(scope_id, "instruction_payload", next_path, ordinal);
                    walk((*instr)->payload, child_scope_id, next_path);
                }
                ++ordinal;
            }
            return;
        }

        if (const auto* list = std::get_if<Value::List>(&value.data)) {
            for (size_t i = 0; i < list->size(); ++i) {
                walk((*list)[i], scope_id, listItemPath(path, i));
            }
            return;
        }

        if (const auto* instr = std::get_if<Value::InstrPtr>(&value.data);
            instr != nullptr && *instr) {
            walk((*instr)->payload, scope_id, path);
        }
    }
};

}  // namespace

Value::Object buildNormalizedRetainedSymbolPayload(const Instruction& root) {
    RetainedSymbolBuilder builder;
    return builder.build(root);
}

bool encodeContainer(const Container& container, std::vector<uint8_t>& out, std::string& err) {
    ContainerHeader header = container.header;
    std::vector<uint8_t> module_data, symbols_data, retained_symbols_data, constants_data;
    std::vector<uint8_t> bytecode_data = container.bytecode_stream;
    std::vector<uint8_t> deps = container.dependencies;
    std::vector<uint8_t> debug = container.debug_info;
    std::vector<uint8_t> integrity = container.integrity;

    encodeModuleMetadata(container.metadata, module_data, err);
    encodeSymbolTable(container.symbols, symbols_data);
    if (!container.retained_symbol_payload.empty() &&
        !encodeRetainedSymbolPayload(container.retained_symbol_payload,
                                     retained_symbols_data,
                                     err)) {
        return false;
    }
    encodeConstantPool(container.constants, constants_data, err);

    std::vector<SectionEntry> sections;
    sections.reserve(8);
    auto addSection = [&](uint16_t id, const std::vector<uint8_t>& data, uint16_t flags) {
        SectionEntry e{}; e.section_id = id; e.section_flags = flags; e.offset = 0; e.length = data.size();
        sections.push_back(e);
    };
    addSection(SECTION_MODULE_METADATA, module_data, 0);
    addSection(SECTION_SYMBOL_TABLE, symbols_data, 0);
    if (!retained_symbols_data.empty()) {
        addSection(SECTION_RETAINED_SYMBOLS, retained_symbols_data, 0);
    }
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
    body.reserve(module_data.size() + symbols_data.size() + retained_symbols_data.size() +
                 constants_data.size() + bytecode_data.size());

    auto writeSection = [&](SectionEntry& s, const std::vector<uint8_t>& data) {
        padTo8(body);
        s.offset = header_size + body.size();
        body.insert(body.end(), data.begin(), data.end());
    };

    writeSection(sections[0], module_data);
    writeSection(sections[1], symbols_data);
    size_t idx = 2;
    if (!retained_symbols_data.empty()) {
        writeSection(sections[idx++], retained_symbols_data);
    }
    writeSection(sections[idx++], constants_data);
    writeSection(sections[idx++], bytecode_data);
    if (!deps.empty()) writeSection(sections[idx++], deps);
    if (!debug.empty()) writeSection(sections[idx++], debug);
    if (!integrity.empty()) writeSection(sections[idx++], integrity);

    // patch header_size and container_size
    size_t container_size = header_size + body.size();

    // patch header bytes
    constexpr size_t kHeaderSizeOffset = 4 + (2 * 5); // magic + 5x u16 fields before header_size
    constexpr size_t kContainerSizeOffset = kHeaderSizeOffset + 2;
    header_bytes[kHeaderSizeOffset] = static_cast<uint8_t>(header_size & 0xFF);
    header_bytes[kHeaderSizeOffset + 1] = static_cast<uint8_t>((header_size >> 8) & 0xFF);
    uint64_t cs = static_cast<uint64_t>(container_size);
    for (int i = 0; i < 8; ++i) {
        header_bytes[kContainerSizeOffset + i] = static_cast<uint8_t>((cs >> (i * 8)) & 0xFF);
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
    std::memcpy(out.header.magic, "SBL3", 4);
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

    out.retained_symbol_payload.clear();
    std::vector<uint8_t> module_data, symbols_data, retained_symbols_data, constants_data;
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
    if (readSection(SECTION_RETAINED_SYMBOLS, retained_symbols_data)) {
        tmp = 0;
        if (!decodeRetainedSymbolPayload(retained_symbols_data.data(),
                                         retained_symbols_data.size(),
                                         tmp,
                                         out.retained_symbol_payload,
                                         err)) {
            return false;
        }
    }
    tmp = 0;
    if (!decodeConstantPool(constants_data.data(), constants_data.size(), tmp, out.constants, err)) return false;

    readSection(SECTION_DEPENDENCIES, out.dependencies);
    readSection(SECTION_DEBUG_INFO, out.debug_info);
    readSection(SECTION_INTEGRITY, out.integrity);
    return true;
}

}  // namespace scratchbird::sblr::v3
