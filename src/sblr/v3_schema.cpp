#include "scratchbird/sblr/v3_schema.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

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
    if (name == "SCHEMA_EMPTY" || name == "SCHEMA_PSQL_SUSPEND") {
        static const SchemaDef empty_schema{"SCHEMA_EMPTY", {}};
        return &empty_schema;
    }

    if (name.rfind("SCHEMA_LITERAL_", 0) == 0) {
        static std::mutex literal_cache_mutex;
        static std::unordered_map<std::string, std::shared_ptr<SchemaDef>> literal_cache;

        std::string key(name);
        std::lock_guard<std::mutex> lock(literal_cache_mutex);
        auto cached = literal_cache.find(key);
        if (cached != literal_cache.end()) {
            return cached->second.get();
        }

        auto schema = std::make_shared<SchemaDef>();
        schema->name = key;

        std::string suffix(name.substr(std::string("SCHEMA_LITERAL_").size()));
        if (suffix == "NULL") {
            // NULL literal has no payload fields.
        } else if (suffix == "DATE" || suffix == "TIME" || suffix == "TIMESTAMP" ||
                   suffix == "TIME_TZ" || suffix == "TIMESTAMP_TZ") {
            schema->fields.push_back(FieldDef{"value", FieldType::I64, ""});
            schema->fields.push_back(FieldDef{"offset_seconds", FieldType::I32, ""});
        } else if (suffix == "DATE32") {
            schema->fields.push_back(FieldDef{"value", FieldType::I32, ""});
        } else {
            schema->fields.push_back(FieldDef{"value", literalFieldType(suffix), ""});
        }

        auto [it, inserted] = literal_cache.emplace(key, std::move(schema));
        (void)inserted;
        return it->second.get();
    }
    return lookupSchemaGenerated(name);
}

}  // namespace scratchbird::sblr::v3
