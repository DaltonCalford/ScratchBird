#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace scratchbird::sblr::v3 {

enum class FieldType : uint8_t {
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    U128,
    UUID,
    F32,
    F64,
    BOOL,
    VARUINT,
    STRING,
    IDENT,
    BYTES,
    SCHEMA_PATH,
    TYPE_SPEC,
    EXPR,
    STMT,
    EXPR_LIST,
    STMT_LIST,
    LIST,
    OPT,
    SCHEMA
};

struct FieldDef {
    std::string name;
    FieldType type;
    std::string ref;  // subtype or schema name
};

struct SchemaDef {
    std::string name;
    std::vector<FieldDef> fields;
};

const SchemaDef* lookupSchema(std::string_view name);

}  // namespace scratchbird::sblr::v3
