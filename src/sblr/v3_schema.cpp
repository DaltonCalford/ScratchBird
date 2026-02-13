#include "scratchbird/sblr/v3_schema.h"

#include <string>

namespace scratchbird::sblr::v3 {

const SchemaDef* lookupSchemaGenerated(std::string_view name);

static FieldType literalFieldType(std::string_view suffix) {
    if (suffix == "BOOLEAN") return FieldType::BOOL;
    if (suffix == "INT8") return FieldType::I8;
    if (suffix == "INT16") return FieldType::I16;
    if (suffix == "INT32") return FieldType::I32;
    if (suffix == "INT64") return FieldType::I64;
    if (suffix == "INT128") return FieldType::U128;
    if (suffix == "UINT8") return FieldType::U8;
    if (suffix == "UINT16") return FieldType::U16;
    if (suffix == "UINT32") return FieldType::U32;
    if (suffix == "UINT64") return FieldType::U64;
    if (suffix == "UINT128") return FieldType::U128;
    if (suffix == "FLOAT32") return FieldType::F32;
    if (suffix == "FLOAT64") return FieldType::F64;
    if (suffix == "DOUBLE") return FieldType::F64;
    if (suffix == "UUID") return FieldType::UUID;
    if (suffix == "STRING") return FieldType::STRING;
    if (suffix == "BINARY") return FieldType::BYTES;
    if (suffix == "JSON") return FieldType::BYTES;
    if (suffix == "JSONB") return FieldType::BYTES;
    if (suffix == "DATE") return FieldType::SCHEMA;
    if (suffix == "TIME") return FieldType::SCHEMA;
    if (suffix == "TIMESTAMP") return FieldType::SCHEMA;
    if (suffix == "TIME_TZ") return FieldType::SCHEMA;
    if (suffix == "TIMESTAMP_TZ") return FieldType::SCHEMA;
    if (suffix == "TSVECTOR") return FieldType::BYTES;
    if (suffix == "TSQUERY") return FieldType::BYTES;
    if (suffix == "BLOB") return FieldType::BYTES;
    if (suffix == "YEAR") return FieldType::I32;
    if (suffix == "MEDIUMINT") return FieldType::I32;
    return FieldType::BYTES;
}

const SchemaDef* lookupSchema(std::string_view name) {
    if (name.rfind("SCHEMA_LITERAL_", 0) == 0) {
        static SchemaDef literal_schema;
        static std::string last_name;
        std::string suffix(name.substr(std::string("SCHEMA_LITERAL_").size()));
        if (last_name != name) {
            literal_schema.name = std::string(name);
            literal_schema.fields.clear();
            if (suffix == "NULL") {
                // NULL literal has no payload fields.
            } else if (suffix == "DATE" || suffix == "TIME" || suffix == "TIMESTAMP" || suffix == "TIME_TZ" || suffix == "TIMESTAMP_TZ") {
                literal_schema.fields.push_back(FieldDef{"value", FieldType::I64, ""});
                literal_schema.fields.push_back(FieldDef{"offset_seconds", FieldType::I32, ""});
            } else if (suffix == "DATE32") {
                literal_schema.fields.push_back(FieldDef{"value", FieldType::I32, ""});
            } else {
                literal_schema.fields.push_back(FieldDef{"value", literalFieldType(suffix), ""});
            }
            last_name = std::string(name);
        }
        return &literal_schema;
    }
    return lookupSchemaGenerated(name);
}

}  // namespace scratchbird::sblr::v3
