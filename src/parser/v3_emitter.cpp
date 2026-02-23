#include "scratchbird/parser/v3_emitter.h"

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_opcode_registry.h"

namespace scratchbird::parser::v3 {

namespace {

using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::TypeSpec;
using scratchbird::sblr::v3::Value;
using scratchbird::sblr::v3::Buffer;
using scratchbird::sblr::v3::DecodeError;

// SBLR3_FUNC_NOW flag bit: 1 means CURRENT_TIMESTAMP semantics (txn-start anchored).
constexpr uint16_t kFuncNowCurrentTimestampFlag = 0x0001;
// SBLR3_FUNC_JSON_EXISTS mode flags.
constexpr uint16_t kFuncJsonExistsAnyFlag = 0x0001;
constexpr uint16_t kFuncJsonExistsAllFlag = 0x0002;

uint16_t op(Opcode opcode) {
    return static_cast<uint16_t>(opcode);
}

std::shared_ptr<Instruction> makeInstr(const Instruction& inst) {
    return std::make_shared<Instruction>(inst);
}

std::string toUpper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

Instruction makeStringLiteralInstruction(std::string text) {
    Instruction lit;
    lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
    lit.flags = 0;
    lit.payload = Value(Value::Object{{"value", Value(std::move(text))}});
    return lit;
}

Instruction makeBoolLiteralInstruction(bool value) {
    Instruction lit;
    lit.opcode = op(Opcode::SBLR3_LITERAL_BOOLEAN);
    lit.flags = 0;
    lit.payload = Value(Value::Object{{"value", Value(value)}});
    return lit;
}

Instruction makeInt64LiteralInstruction(int64_t value) {
    Instruction lit;
    lit.opcode = op(Opcode::SBLR3_LITERAL_INT64);
    lit.flags = 0;
    lit.payload = Value(Value::Object{{"value", Value(value)}});
    return lit;
}

Instruction makeDoubleLiteralInstruction(double value) {
    Instruction lit;
    lit.opcode = op(Opcode::SBLR3_LITERAL_DOUBLE);
    lit.flags = 0;
    lit.payload = Value(Value::Object{{"value", Value(value)}});
    return lit;
}

bool parseBoolToken(std::string_view raw, bool& out) {
    std::string normalized = toUpper(raw);
    if (normalized == "TRUE" || normalized == "ON") {
        out = true;
        return true;
    }
    if (normalized == "FALSE" || normalized == "OFF") {
        out = false;
        return true;
    }
    return false;
}

bool parseInt64Token(std::string_view raw, int64_t& out) {
    if (raw.empty()) {
        return false;
    }
    const char* begin = raw.data();
    const char* end = raw.data() + raw.size();
    auto result = std::from_chars(begin, end, out, 10);
    return result.ec == std::errc() && result.ptr == end;
}

bool parseDoubleToken(std::string_view raw, double& out) {
    if (raw.empty()) {
        return false;
    }
    std::string tmp(raw);
    char* parse_end = nullptr;
    errno = 0;
    out = std::strtod(tmp.c_str(), &parse_end);
    if (parse_end == tmp.c_str() || *parse_end != '\0' || errno == ERANGE) {
        return false;
    }
    return true;
}

uint64_t privilegeToCatalogMask(PrivilegeType privilege) {
    using CatalogPrivilege = scratchbird::core::CatalogManager::Privilege;
    switch (privilege) {
        case PrivilegeType::SELECT:
            return static_cast<uint64_t>(CatalogPrivilege::SELECT);
        case PrivilegeType::INSERT:
            return static_cast<uint64_t>(CatalogPrivilege::INSERT);
        case PrivilegeType::UPDATE:
            return static_cast<uint64_t>(CatalogPrivilege::UPDATE);
        case PrivilegeType::DELETE:
            return static_cast<uint64_t>(CatalogPrivilege::DELETE);
        case PrivilegeType::TRUNCATE:
            return static_cast<uint64_t>(CatalogPrivilege::TRUNCATE);
        case PrivilegeType::REFERENCES:
            return static_cast<uint64_t>(CatalogPrivilege::REFERENCES);
        case PrivilegeType::TRIGGER:
            return static_cast<uint64_t>(CatalogPrivilege::TRIGGER);
        case PrivilegeType::EXECUTE:
            return static_cast<uint64_t>(CatalogPrivilege::EXECUTE);
        case PrivilegeType::USAGE:
            return static_cast<uint64_t>(CatalogPrivilege::USAGE);
        case PrivilegeType::COPY:
            return static_cast<uint64_t>(CatalogPrivilege::COPY_FILE);
        case PrivilegeType::CREATE_JOB:
            return static_cast<uint64_t>(CatalogPrivilege::CREATE_JOB);
        case PrivilegeType::VIEW_JOB_HISTORY:
            return static_cast<uint64_t>(CatalogPrivilege::VIEW_JOB_HISTORY);
        case PrivilegeType::EXECUTE_EXTERNAL_JOB:
            return static_cast<uint64_t>(CatalogPrivilege::EXECUTE_EXTERNAL_JOB);
        case PrivilegeType::CREATE:
            return static_cast<uint64_t>(CatalogPrivilege::CREATE);
        case PrivilegeType::CONNECT:
            return static_cast<uint64_t>(CatalogPrivilege::CONNECT);
        case PrivilegeType::TEMPORARY:
            return static_cast<uint64_t>(CatalogPrivilege::TEMPORARY);
        case PrivilegeType::ALL:
            return static_cast<uint64_t>(CatalogPrivilege::ALL);
        default:
            return 0;
    }
}

Instruction makeIndexOptionValueInstruction(std::string_view raw) {
    bool bool_value = false;
    if (parseBoolToken(raw, bool_value)) {
        return makeBoolLiteralInstruction(bool_value);
    }
    int64_t int_value = 0;
    if (parseInt64Token(raw, int_value)) {
        return makeInt64LiteralInstruction(int_value);
    }
    double double_value = 0.0;
    if (parseDoubleToken(raw, double_value)) {
        return makeDoubleLiteralInstruction(double_value);
    }
    return makeStringLiteralInstruction(std::string(raw));
}

Value makeOptionKvFromAssignments(parser::v3::StringPool& pool,
                                  const std::vector<parser::v3::IndexOptionAssignment>& assignments) {
    Value::List options;
    options.reserve(assignments.size());
    for (const auto& assignment : assignments) {
        Value::Object option;
        option["key"] = Value(toUpper(pool.get(assignment.option_name)));
        option["value"] = Value(makeInstr(makeIndexOptionValueInstruction(pool.get(assignment.option_value))));
        options.push_back(Value(std::move(option)));
    }
    return Value(std::move(options));
}

Value makeOptionKvFromResetList(parser::v3::StringPool& pool,
                                const std::vector<parser::v3::StringPool::StringId>& reset_options,
                                const Instruction& default_value) {
    Value::List options;
    options.reserve(reset_options.size());
    for (auto option_id : reset_options) {
        Value::Object option;
        option["key"] = Value(toUpper(pool.get(option_id)));
        option["value"] = Value(makeInstr(default_value));
        options.push_back(Value(std::move(option)));
    }
    return Value(std::move(options));
}

uint8_t mapJoinType(parser::JoinType type) {
    switch (type) {
        case parser::JoinType::INNER: return 1;
        case parser::JoinType::LEFT: return 2;
        case parser::JoinType::RIGHT: return 3;
        case parser::JoinType::FULL: return 4;
        case parser::JoinType::CROSS: return 5;
        case parser::JoinType::NATURAL: return 1;
        case parser::JoinType::NATURAL_LEFT: return 2;
        case parser::JoinType::NATURAL_RIGHT: return 3;
        case parser::JoinType::NATURAL_FULL: return 4;
    }
    return 1;
}

uint8_t mapSortOrder(bool ascending) {
    return ascending ? 0 : 1;
}

uint8_t mapNullsOrder(const parser::v3::OrderByItem* item) {
    if (!item->has_nulls_spec) return 0;
    return item->nulls_first ? 1 : 2;
}

uint8_t mapGroupingType(parser::GroupingType type) {
    switch (type) {
        case parser::GroupingType::STANDARD: return 0;
        case parser::GroupingType::ROLLUP: return 1;
        case parser::GroupingType::CUBE: return 2;
        case parser::GroupingType::GROUPING_SETS: return 3;
    }
    return 0;
}

std::string indexTypeName(parser::v3::IndexType type) {
    switch (type) {
        case parser::v3::IndexType::BTREE: return "BTREE";
        case parser::v3::IndexType::HASH: return "HASH";
        case parser::v3::IndexType::GIN: return "GIN";
        case parser::v3::IndexType::GIST: return "GIST";
        case parser::v3::IndexType::SPGIST: return "SPGIST";
        case parser::v3::IndexType::BRIN: return "BRIN";
        case parser::v3::IndexType::RTREE: return "RTREE";
        case parser::v3::IndexType::HNSW: return "HNSW";
        case parser::v3::IndexType::BITMAP: return "BITMAP";
        case parser::v3::IndexType::COLUMNSTORE: return "COLUMNSTORE";
        case parser::v3::IndexType::LSM: return "LSM";
        case parser::v3::IndexType::FULLTEXT: return "FULLTEXT";
        case parser::v3::IndexType::IVF: return "IVF";
        case parser::v3::IndexType::ZONEMAP: return "ZONEMAP";
        case parser::v3::IndexType::ART: return "ART";
        case parser::v3::IndexType::BLOOM: return "BLOOM";
        case parser::v3::IndexType::VECTOR_FLAT: return "VECTOR_FLAT";
        case parser::v3::IndexType::VECTOR_BIN_FLAT: return "VECTOR_BIN_FLAT";
        case parser::v3::IndexType::IVF_FLAT: return "IVF_FLAT";
        case parser::v3::IndexType::BIN_IVF_FLAT: return "BIN_IVF_FLAT";
        case parser::v3::IndexType::IVF_PQ: return "IVF_PQ";
        case parser::v3::IndexType::IVF_SQ8: return "IVF_SQ8";
        case parser::v3::IndexType::IVF_SQ8_HYBRID: return "IVF_SQ8_HYBRID";
        case parser::v3::IndexType::RHNSW_PQ: return "RHNSW_PQ";
        case parser::v3::IndexType::RHNSW_SQ: return "RHNSW_SQ";
        case parser::v3::IndexType::ANNOY: return "ANNOY";
        case parser::v3::IndexType::NSG: return "NSG";
        case parser::v3::IndexType::DISKANN: return "DISKANN";
        case parser::v3::IndexType::SCANN: return "SCANN";
        case parser::v3::IndexType::GPU_CAGRA: return "GPU_CAGRA";
        case parser::v3::IndexType::MINHASH_LSH: return "MINHASH_LSH";
        case parser::v3::IndexType::SPARSE_INVERTED: return "SPARSE_INVERTED";
        case parser::v3::IndexType::SPARSE_WAND: return "SPARSE_WAND";
        case parser::v3::IndexType::TRIE: return "TRIE";
        case parser::v3::IndexType::INVERTED: return "INVERTED";
        case parser::v3::IndexType::STL_SORT: return "STL_SORT";
        case parser::v3::IndexType::NGRAM: return "NGRAM";
        case parser::v3::IndexType::MONGODB_2D: return "MONGODB_2D";
        case parser::v3::IndexType::MONGODB_2DSPHERE: return "MONGODB_2DSPHERE";
        case parser::v3::IndexType::MONGODB_2DSPHERE_BUCKET: return "MONGODB_2DSPHERE_BUCKET";
        case parser::v3::IndexType::MONGODB_GEO_HAYSTACK: return "MONGODB_GEO_HAYSTACK";
        case parser::v3::IndexType::MONGODB_WILDCARD: return "MONGODB_WILDCARD";
        case parser::v3::IndexType::MONGODB_ENCRYPTED_RANGE: return "MONGODB_ENCRYPTED_RANGE";
        case parser::v3::IndexType::NEO4J_LOOKUP: return "NEO4J_LOOKUP";
        case parser::v3::IndexType::NEO4J_TEXT: return "NEO4J_TEXT";
        case parser::v3::IndexType::NEO4J_RANGE: return "NEO4J_RANGE";
        case parser::v3::IndexType::NEO4J_POINT: return "NEO4J_POINT";
        case parser::v3::IndexType::NEO4J_VECTOR: return "NEO4J_VECTOR";
        case parser::v3::IndexType::CASSANDRA_SASI: return "CASSANDRA_SASI";
        case parser::v3::IndexType::CASSANDRA_SAI: return "CASSANDRA_SAI";
        case parser::v3::IndexType::REDIS_STRING: return "REDIS_STRING";
        case parser::v3::IndexType::REDIS_HASH: return "REDIS_HASH";
        case parser::v3::IndexType::REDIS_LIST: return "REDIS_LIST";
        case parser::v3::IndexType::REDIS_SET: return "REDIS_SET";
        case parser::v3::IndexType::REDIS_ZSET: return "REDIS_ZSET";
        case parser::v3::IndexType::REDIS_STREAM: return "REDIS_STREAM";
        case parser::v3::IndexType::REDIS_BITMAP: return "REDIS_BITMAP";
        case parser::v3::IndexType::REDIS_HLL: return "REDIS_HLL";
        case parser::v3::IndexType::REDIS_GEO: return "REDIS_GEO";
    }
    return "BTREE";
}

uint8_t mapFrameUnit(parser::v3::FrameType type) {
    switch (type) {
        case parser::v3::FrameType::ROWS: return 0;
        case parser::v3::FrameType::RANGE: return 1;
        case parser::v3::FrameType::GROUPS: return 2;
    }
    return 0;
}

uint8_t mapFrameBound(parser::v3::FrameBoundType type) {
    switch (type) {
        case parser::v3::FrameBoundType::UNBOUNDED_PRECEDING: return 0;
        case parser::v3::FrameBoundType::VALUE_PRECEDING: return 1;
        case parser::v3::FrameBoundType::CURRENT_ROW: return 2;
        case parser::v3::FrameBoundType::VALUE_FOLLOWING: return 3;
        case parser::v3::FrameBoundType::UNBOUNDED_FOLLOWING: return 4;
    }
    return 0;
}

void appendLE16(uint16_t v, Value::Bytes& out) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void appendLE32(uint32_t v, Value::Bytes& out) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void appendLE64(uint64_t v, Value::Bytes& out) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 32) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 40) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 48) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 56) & 0xFF));
}

void appendVaruint(uint64_t v, Value::Bytes& out) {
    Buffer tmp;
    scratchbird::sblr::v3::encodeVaruint(v, tmp);
    out.insert(out.end(), tmp.begin(), tmp.end());
}

void appendBytesWithLen(const Value::Bytes& bytes, Value::Bytes& out) {
    appendVaruint(bytes.size(), out);
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void appendStringWithLen(std::string_view s, Value::Bytes& out) {
    appendVaruint(s.size(), out);
    out.insert(out.end(), s.begin(), s.end());
}

std::string schemaPathToString(parser::v3::StringPool& pool,
                               const parser::v3::SchemaPath& path) {
    std::ostringstream oss;
    bool first = true;
    for (auto id : path.components) {
        if (id == parser::v3::StringPool::INVALID_ID) {
            continue;
        }
        if (!first) {
            oss << '.';
        }
        oss << pool.get(id);
        first = false;
    }
    return oss.str();
}

std::string columnRefToString(parser::v3::StringPool& pool,
                              const parser::v3::ColumnRef& ref) {
    std::ostringstream oss;
    if (ref.has_table_qualifier) {
        std::string table_name = schemaPathToString(pool, ref.table_path);
        if (!table_name.empty()) {
            oss << table_name << '.';
        }
    }
    if (ref.column_name == parser::v3::StringPool::INVALID_ID) {
        return {};
    }
    oss << pool.get(ref.column_name);
    return oss.str();
}

std::string renderSimpleSelectDefinition(parser::v3::StringPool& pool,
                                         parser::v3::Statement* query_stmt) {
    if (!query_stmt || query_stmt->kind() != parser::v3::ASTKind::SelectStmt) {
        return {};
    }

    auto* select = static_cast<parser::v3::SelectStmt*>(query_stmt);
    if (select->with || select->distinct || select->all || !select->joins.empty() ||
        select->where || !select->group_by.empty() || select->having ||
        !select->order_by.empty() || select->limit || select->offset ||
        select->set_op != parser::v3::SetOpType::NONE || select->for_update ||
        select->for_share) {
        return {};
    }
    if (!select->from || select->from->ref_type != parser::v3::TableRefNode::Type::TABLE) {
        return {};
    }

    std::vector<std::string> select_items;
    select_items.reserve(select->items.size());
    for (const auto* item : select->items) {
        if (!item) {
            return {};
        }
        if (item->item_type == parser::v3::SelectItem::Type::STAR) {
            select_items.emplace_back("*");
            continue;
        }
        if (item->item_type == parser::v3::SelectItem::Type::TABLE_STAR) {
            std::string table_star = schemaPathToString(pool, item->table_path);
            if (table_star.empty()) {
                return {};
            }
            select_items.push_back(table_star + ".*");
            continue;
        }
        if (item->item_type != parser::v3::SelectItem::Type::EXPRESSION || !item->expr ||
            item->expr->kind() != parser::v3::ASTKind::ColumnRefExpr) {
            return {};
        }
        const auto* col_ref = static_cast<parser::v3::ColumnRefExpr*>(item->expr);
        std::string col_name = columnRefToString(pool, col_ref->column);
        if (col_name.empty()) {
            return {};
        }
        select_items.push_back(col_name);
    }

    if (select_items.empty()) {
        return {};
    }

    std::string from_name = schemaPathToString(pool, select->from->table_path);
    if (from_name.empty()) {
        return {};
    }

    std::ostringstream oss;
    oss << "SELECT ";
    for (size_t i = 0; i < select_items.size(); ++i) {
        if (i != 0) {
            oss << ", ";
        }
        oss << select_items[i];
    }
    oss << " FROM " << from_name;
    return oss.str();
}

}  // namespace

V3Emitter::V3Emitter(parser::v3::StringPool& pool)
    : pool_(pool) {}

bool V3Emitter::emitVNextContractInstruction(
    parser::v3::ASTKind node_kind,
    const scratchbird::sblr::v3::Value::Object& fields,
    scratchbird::sblr::v3::Instruction& out,
    std::string& err) {
    using scratchbird::sblr::v3::Opcode;

    auto requireU64 = [&](const char* key, uint64_t& value_out) -> bool {
        auto it = fields.find(key);
        if (it == fields.end()) {
            err = std::string("IRX_0402: missing required field ") + key;
            return false;
        }
        const auto* value = std::get_if<uint64_t>(&it->second.data);
        if (!value) {
            err = std::string("IRX_0402: field type mismatch for ") + key;
            return false;
        }
        value_out = *value;
        return true;
    };

    auto requireString = [&](const char* key, std::string& value_out) -> bool {
        auto it = fields.find(key);
        if (it == fields.end()) {
            err = std::string("IRX_0402: missing required field ") + key;
            return false;
        }
        const auto* value = std::get_if<std::string>(&it->second.data);
        if (!value) {
            err = std::string("IRX_0402: field type mismatch for ") + key;
            return false;
        }
        value_out = *value;
        return true;
    };

    auto requireList = [&](const char* key, scratchbird::sblr::v3::Value::List& value_out) -> bool {
        auto it = fields.find(key);
        if (it == fields.end()) {
            err = std::string("IRX_0402: missing required field ") + key;
            return false;
        }
        const auto* value = std::get_if<scratchbird::sblr::v3::Value::List>(&it->second.data);
        if (!value) {
            err = std::string("IRX_0402: field type mismatch for ") + key;
            return false;
        }
        value_out = *value;
        return true;
    };

    auto requireBytes = [&](const char* key, scratchbird::sblr::v3::Value::Bytes& value_out) -> bool {
        auto it = fields.find(key);
        if (it == fields.end()) {
            err = std::string("IRX_0402: missing required field ") + key;
            return false;
        }
        const auto* value = std::get_if<scratchbird::sblr::v3::Value::Bytes>(&it->second.data);
        if (!value) {
            err = std::string("IRX_0402: field type mismatch for ") + key;
            return false;
        }
        value_out = *value;
        return true;
    };

    out.flags = 0;
    scratchbird::sblr::v3::Value::Object payload;

    switch (node_kind) {
        case parser::v3::ASTKind::AST_DOC_PATH_FILTER: {
            uint64_t path_expr = 0;
            uint64_t compare_op = 0;
            uint64_t value_expr = 0;
            if (!requireU64("path_expr", path_expr) ||
                !requireU64("operator", compare_op) ||
                !requireU64("value_expr", value_expr)) {
                return false;
            }
            out.opcode = op(Opcode::SBLR3_OP_DOC_PATH_FILTER);
            payload["path_id"] = scratchbird::sblr::v3::Value(path_expr);
            payload["cmp"] = scratchbird::sblr::v3::Value(compare_op);
            payload["value_ref"] = scratchbird::sblr::v3::Value(value_expr);
            break;
        }
        case parser::v3::ASTKind::AST_TS_BUCKET_AGG: {
            uint64_t time_expr = 0;
            uint64_t bucket_size = 0;
            scratchbird::sblr::v3::Value::List agg_list;
            if (!requireU64("time_expr", time_expr) ||
                !requireU64("bucket_size", bucket_size) ||
                !requireList("agg_list", agg_list)) {
                return false;
            }
            scratchbird::sblr::v3::Value::Bytes agg_ref_bytes;
            agg_ref_bytes.reserve(agg_list.size() * sizeof(uint32_t));
            for (const auto& agg_ref : agg_list) {
                const auto* id = std::get_if<uint64_t>(&agg_ref.data);
                if (!id) {
                    err = "IRX_0402: agg_list entries must be uint64";
                    return false;
                }
                appendLE32(static_cast<uint32_t>(*id), agg_ref_bytes);
            }
            out.opcode = op(Opcode::SBLR3_OP_TS_BUCKET_AGG);
            payload["time_expr"] = scratchbird::sblr::v3::Value(time_expr);
            payload["bucket_ns"] = scratchbird::sblr::v3::Value(bucket_size);
            payload["agg_count"] = scratchbird::sblr::v3::Value(static_cast<uint64_t>(agg_list.size()));
            payload["agg_refs"] = scratchbird::sblr::v3::Value(std::move(agg_ref_bytes));
            break;
        }
        case parser::v3::ASTKind::AST_COL_SCAN_HINT: {
            uint64_t table_ref = 0;
            scratchbird::sblr::v3::Value::Bytes projection_set;
            scratchbird::sblr::v3::Value::Bytes predicate_set;
            if (!requireU64("table_ref", table_ref) ||
                !requireBytes("projection_set", projection_set) ||
                !requireBytes("predicate_set", predicate_set)) {
                return false;
            }
            out.opcode = op(Opcode::SBLR3_OP_COL_SCAN);
            payload["table_id"] = scratchbird::sblr::v3::Value(table_ref);
            payload["proj_bitmap"] = scratchbird::sblr::v3::Value(std::move(projection_set));
            payload["predicate_bitmap"] = scratchbird::sblr::v3::Value(std::move(predicate_set));
            break;
        }
        case parser::v3::ASTKind::AST_SEARCH_QUERY_DSL: {
            std::string dsl_payload;
            uint64_t target_index = 0;
            uint64_t scorer_id = 1;  // BM25 default
            if (!requireString("dsl_payload_json", dsl_payload) ||
                !requireU64("target_index", target_index)) {
                return false;
            }
            auto scorer_it = fields.find("scorer_id");
            if (scorer_it != fields.end()) {
                const auto* scorer_value = std::get_if<uint64_t>(&scorer_it->second.data);
                if (!scorer_value) {
                    err = "IRX_0402: field type mismatch for scorer_id";
                    return false;
                }
                scorer_id = *scorer_value;
            }
            out.opcode = op(Opcode::SBLR3_OP_SEARCH_DSL_EVAL);
            payload["dsl_payload_json"] = scratchbird::sblr::v3::Value(dsl_payload);
            payload["target_index"] = scratchbird::sblr::v3::Value(target_index);
            payload["dsl_blob_ref"] = scratchbird::sblr::v3::Value(target_index);
            payload["scorer_id"] = scratchbird::sblr::v3::Value(scorer_id);
            break;
        }
        case parser::v3::ASTKind::AST_VECTOR_ANN_QUERY: {
            uint64_t vector_expr = 0;
            uint64_t metric = 0;
            uint64_t k = 0;
            uint64_t ef_search = 0;
            if (!requireU64("vector_expr", vector_expr) ||
                !requireU64("metric", metric) ||
                !requireU64("k", k) ||
                !requireU64("ef_search", ef_search)) {
                return false;
            }
            out.opcode = op(Opcode::SBLR3_OP_VECTOR_ANN);
            payload["index_id"] = scratchbird::sblr::v3::Value(vector_expr);
            payload["metric"] = scratchbird::sblr::v3::Value(metric);
            payload["topk"] = scratchbird::sblr::v3::Value(k);
            payload["ef"] = scratchbird::sblr::v3::Value(ef_search);
            break;
        }
        case parser::v3::ASTKind::AST_HYBRID_BRIDGE: {
            uint64_t source_track = 0;
            uint64_t target_track = 0;
            uint64_t bridge_mode = 0;
            if (!requireU64("source_track", source_track) ||
                !requireU64("target_track", target_track) ||
                !requireU64("bridge_mode", bridge_mode)) {
                return false;
            }
            out.opcode = op(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE);
            payload["src_track"] = scratchbird::sblr::v3::Value(source_track);
            payload["dst_track"] = scratchbird::sblr::v3::Value(target_track);
            payload["mode"] = scratchbird::sblr::v3::Value(bridge_mode);
            break;
        }
        case parser::v3::ASTKind::AST_UDR_COMPILE_DISPATCH: {
            uint64_t validate_only = 0;
            std::string profile_id;
            std::string payload_format;
            std::string payload_bytes;
            std::string session_signature;
            if (!requireU64("validate_only", validate_only) ||
                !requireString("profile_id", profile_id) ||
                !requireString("payload_format", payload_format) ||
                !requireString("payload_bytes", payload_bytes) ||
                !requireString("session_signature", session_signature)) {
                return false;
            }
            if (validate_only > 1) {
                err = "IRX_0407: validate_only enum out of range";
                return false;
            }
            out.opcode = op(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH);
            payload["validate_only"] = scratchbird::sblr::v3::Value(validate_only == 1);
            payload["profile_id"] = scratchbird::sblr::v3::Value(std::move(profile_id));
            payload["payload_format"] = scratchbird::sblr::v3::Value(std::move(payload_format));
            payload["payload_bytes"] = scratchbird::sblr::v3::Value(std::move(payload_bytes));
            payload["session_signature"] = scratchbird::sblr::v3::Value(std::move(session_signature));
            break;
        }
        case parser::v3::ASTKind::AST_UDR_EMBEDDED_SQL_COMPILE: {
            uint64_t validate_only = 0;
            std::string template_id;
            std::string sql_text;
            std::string profile_id;
            std::string session_signature;
            if (!requireU64("validate_only", validate_only) ||
                !requireString("template_id", template_id) ||
                !requireString("sql_text", sql_text) ||
                !requireString("profile_id", profile_id) ||
                !requireString("session_signature", session_signature)) {
                return false;
            }
            if (validate_only > 1) {
                err = "IRX_0407: validate_only enum out of range";
                return false;
            }
            out.opcode = op(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE);
            payload["validate_only"] = scratchbird::sblr::v3::Value(validate_only == 1);
            payload["template_id"] = scratchbird::sblr::v3::Value(std::move(template_id));
            payload["sql_text"] = scratchbird::sblr::v3::Value(std::move(sql_text));
            payload["profile_id"] = scratchbird::sblr::v3::Value(std::move(profile_id));
            payload["session_signature"] = scratchbird::sblr::v3::Value(std::move(session_signature));
            break;
        }
        default:
            err = "IRX_0401: unknown AST vNext node";
            return false;
    }

    out.payload = scratchbird::sblr::v3::Value(std::move(payload));
    return true;
}

bool V3Emitter::emitStatementToContainer(parser::v3::Statement* stmt,
                                         scratchbird::sblr::v3::Container& out,
                                         std::string& err) {
    ok_ = true;
    error_.clear();

    Instruction root = emitStatement(stmt);
    if (!ok_) {
        err = error_;
        return false;
    }

    scratchbird::sblr::v3::Container container;
    container.header.version_major = 3;
    container.header.version_minor = 0;
    container.header.version_patch = 0;
    container.header.flags = 0;

    container.metadata.module_name = "scratchbird";
    container.metadata.module_version = "v3";
    container.metadata.dialect_id = 1;
    container.metadata.target_platform = 0;
    container.metadata.build_id = "";
    container.metadata.source_hash = {};

    scratchbird::sblr::v3::Buffer stream;
    scratchbird::sblr::v3::DecodeError derr;

    // SBLR3_VERSION payload: u16 major, minor, patch
    {
        scratchbird::sblr::v3::Instruction ver;
        ver.opcode = op(Opcode::SBLR3_VERSION);
        ver.flags = 0;
        Value::Bytes bytes;
        bytes.resize(6);
        bytes[0] = 3; bytes[1] = 0;
        bytes[2] = 0; bytes[3] = 0;
        bytes[4] = 0; bytes[5] = 0;
        ver.payload = Value(bytes);
        if (!scratchbird::sblr::v3::encodeInstructionWithSchema(ver, stream, derr)) {
            err = derr.message;
            return false;
        }
    }

    if (!scratchbird::sblr::v3::encodeInstructionWithSchema(root, stream, derr)) {
        err = derr.message;
        return false;
    }

    {
        scratchbird::sblr::v3::Instruction end;
        end.opcode = op(Opcode::SBLR3_END);
        end.flags = 0;
        end.payload = Value(Value::Bytes{});
        if (!scratchbird::sblr::v3::encodeInstructionWithSchema(end, stream, derr)) {
            err = derr.message;
            return false;
        }
    }

    container.bytecode_stream = std::move(stream);
    out = std::move(container);
    return true;
}

void V3Emitter::fail(const std::string& message) {
    if (!ok_) return;
    ok_ = false;
    error_ = message;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitStatement(parser::v3::Statement* stmt) {
    if (!stmt) {
        fail("null statement");
        return {};
    }

    switch (stmt->kind()) {
        case parser::v3::ASTKind::SelectStmt:
            return emitSelect(static_cast<parser::v3::SelectStmt*>(stmt));
        case parser::v3::ASTKind::InsertStmt:
            return emitInsert(static_cast<parser::v3::InsertStmt*>(stmt));
        case parser::v3::ASTKind::UpdateStmt:
            return emitUpdate(static_cast<parser::v3::UpdateStmt*>(stmt));
        case parser::v3::ASTKind::DeleteStmt:
            return emitDelete(static_cast<parser::v3::DeleteStmt*>(stmt));
        case parser::v3::ASTKind::MergeStmt:
            return emitMerge(static_cast<parser::v3::MergeStmt*>(stmt));
        case parser::v3::ASTKind::CopyStmt:
            return emitCopy(static_cast<parser::v3::CopyStmt*>(stmt));
        case parser::v3::ASTKind::CreateTableStmt:
        case parser::v3::ASTKind::CreateIndexStmt:
        case parser::v3::ASTKind::CreateViewStmt:
        case parser::v3::ASTKind::CreateSequenceStmt:
        case parser::v3::ASTKind::CreateSchemaStmt:
        case parser::v3::ASTKind::CreateDatabaseStmt:
        case parser::v3::ASTKind::CreateTablespaceStmt:
        case parser::v3::ASTKind::CreateFunctionStmt:
        case parser::v3::ASTKind::CreateProcedureStmt:
        case parser::v3::ASTKind::CreateTriggerStmt:
        case parser::v3::ASTKind::CreatePackageStmt:
        case parser::v3::ASTKind::CreateExceptionStmt:
        case parser::v3::ASTKind::CreateDomainStmt:
        case parser::v3::ASTKind::CreateTypeStmt:
        case parser::v3::ASTKind::CreateUserStmt:
        case parser::v3::ASTKind::CreateRoleStmt:
        case parser::v3::ASTKind::CreateGroupStmt:
        case parser::v3::ASTKind::CreatePolicyStmt:
        case parser::v3::ASTKind::CreateForeignServerStmt:
        case parser::v3::ASTKind::CreateForeignTableStmt:
        case parser::v3::ASTKind::CreateForeignDataWrapperStmt:
        case parser::v3::ASTKind::CreateUserMappingStmt:
        case parser::v3::ASTKind::CreateSynonymStmt:
        case parser::v3::ASTKind::CreateUdrStmt:
        case parser::v3::ASTKind::CreateJobStmt:
            return emitDdlCreate(stmt);
        case parser::v3::ASTKind::AlterTableStmt:
        case parser::v3::ASTKind::AlterIndexStmt:
        case parser::v3::ASTKind::AlterSequenceStmt:
        case parser::v3::ASTKind::AlterSchemaStmt:
        case parser::v3::ASTKind::AlterDatabaseStmt:
        case parser::v3::ASTKind::AlterTablespaceStmt:
        case parser::v3::ASTKind::AttachTablespaceStmt:
        case parser::v3::ASTKind::DetachTablespaceStmt:
        case parser::v3::ASTKind::AlterDomainStmt:
        case parser::v3::ASTKind::AlterTypeStmt:
        case parser::v3::ASTKind::AlterPolicyStmt:
        case parser::v3::ASTKind::AlterSystemStmt:
        case parser::v3::ASTKind::AlterJobStmt:
        case parser::v3::ASTKind::RenameObjectStmt:
        case parser::v3::ASTKind::MoveObjectStmt:
            return emitDdlAlter(stmt);
        case parser::v3::ASTKind::DropTableStmt:
        case parser::v3::ASTKind::DropIndexStmt:
        case parser::v3::ASTKind::DropViewStmt:
        case parser::v3::ASTKind::DropSequenceStmt:
        case parser::v3::ASTKind::DropSchemaStmt:
        case parser::v3::ASTKind::DropDatabaseStmt:
        case parser::v3::ASTKind::DropTablespaceStmt:
        case parser::v3::ASTKind::DropDomainStmt:
        case parser::v3::ASTKind::DropTypeStmt:
        case parser::v3::ASTKind::DropFunctionStmt:
        case parser::v3::ASTKind::DropProcedureStmt:
        case parser::v3::ASTKind::DropTriggerStmt:
        case parser::v3::ASTKind::DropPackageStmt:
        case parser::v3::ASTKind::DropRoleStmt:
        case parser::v3::ASTKind::DropGroupStmt:
        case parser::v3::ASTKind::DropExceptionStmt:
        case parser::v3::ASTKind::DropForeignServerStmt:
        case parser::v3::ASTKind::DropForeignTableStmt:
        case parser::v3::ASTKind::DropUserMappingStmt:
        case parser::v3::ASTKind::DropSynonymStmt:
        case parser::v3::ASTKind::DropUdrStmt:
        case parser::v3::ASTKind::DropJobStmt:
        case parser::v3::ASTKind::DropUserStmt:
        case parser::v3::ASTKind::DropPolicyStmt:
            return emitDdlDrop(stmt);
        case parser::v3::ASTKind::TruncateTableStmt:
            return emitDdlTruncate(static_cast<parser::v3::TruncateTableStmt*>(stmt));
        case parser::v3::ASTKind::CommentStmt:
            return emitComment(static_cast<parser::v3::CommentStmt*>(stmt));
        case parser::v3::ASTKind::GrantStmt:
            return emitGrant(static_cast<parser::v3::GrantStmt*>(stmt));
        case parser::v3::ASTKind::RevokeStmt:
            return emitRevoke(static_cast<parser::v3::RevokeStmt*>(stmt));
        case parser::v3::ASTKind::StartTransactionStmt:
        case parser::v3::ASTKind::PrepareTransactionStmt:
        case parser::v3::ASTKind::CommitStmt:
        case parser::v3::ASTKind::RollbackStmt:
        case parser::v3::ASTKind::SavepointStmt:
        case parser::v3::ASTKind::ReleaseSavepointStmt:
            return emitTxn(stmt);
        case parser::v3::ASTKind::SetStmt:
        case parser::v3::ASTKind::ResetStmt:
        case parser::v3::ASTKind::ShowStmt:
        case parser::v3::ASTKind::ExplainStmt:
        case parser::v3::ASTKind::AnalyzeStmt:
            return emitSetShowReset(stmt);
        case parser::v3::ASTKind::ConnectStmt:
        case parser::v3::ASTKind::DisconnectStmt:
        case parser::v3::ASTKind::SweepDatabaseStmt:
        case parser::v3::ASTKind::ExecuteJobStmt:
        case parser::v3::ASTKind::CancelJobRunStmt:
        case parser::v3::ASTKind::AST_DOC_PATH_FILTER:
        case parser::v3::ASTKind::AST_TS_BUCKET_AGG:
        case parser::v3::ASTKind::AST_SEARCH_QUERY_DSL:
        case parser::v3::ASTKind::AST_VECTOR_ANN_QUERY:
        case parser::v3::ASTKind::AST_HYBRID_BRIDGE:
        case parser::v3::ASTKind::AST_UDR_COMPILE_DISPATCH:
        case parser::v3::ASTKind::AST_UDR_EMBEDDED_SQL_COMPILE:
            return emitUtility(stmt);
        case parser::v3::ASTKind::ExecuteBlockStmt:
        case parser::v3::ASTKind::CompoundStmt:
        case parser::v3::ASTKind::DeclareVariableStmt:
        case parser::v3::ASTKind::AssignmentStmt:
        case parser::v3::ASTKind::IfStmt:
        case parser::v3::ASTKind::WhileStmt:
        case parser::v3::ASTKind::ForSelectStmt:
        case parser::v3::ASTKind::ForExecuteStmt:
        case parser::v3::ASTKind::LoopStmt:
        case parser::v3::ASTKind::LeaveStmt:
        case parser::v3::ASTKind::ContinueStmt:
        case parser::v3::ASTKind::ExitStmt:
        case parser::v3::ASTKind::SuspendStmt:
        case parser::v3::ASTKind::ReturnStmt:
        case parser::v3::ASTKind::ExceptionRaiseStmt:
        case parser::v3::ASTKind::WhenExceptionStmt:
        case parser::v3::ASTKind::PostEventStmt:
        case parser::v3::ASTKind::DeclareCursorStmt:
        case parser::v3::ASTKind::OpenCursorStmt:
        case parser::v3::ASTKind::FetchCursorStmt:
        case parser::v3::ASTKind::CloseCursorStmt:
        case parser::v3::ASTKind::ExecuteProcedureStmt:
        case parser::v3::ASTKind::ExecuteStatementStmt:
            return emitPsql(stmt);
        default:
            return emitUtility(stmt);
    }
}

scratchbird::sblr::v3::Instruction V3Emitter::emitSelect(parser::v3::SelectStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_SELECT);
    inst.flags = 0;

    Value::Object payload;
    uint16_t flags = 0;
    if (stmt->distinct) flags |= 0x0001;
    if (stmt->all) flags |= 0x0002;
    if (stmt->for_update) flags |= 0x0004;
    if (stmt->for_share) flags |= 0x0008;
    if (stmt->nowait) flags |= 0x0010;
    if (stmt->skip_locked) flags |= 0x0020;
    if (stmt->with_lock) flags |= 0x0040;
    payload["flags"] = Value(static_cast<uint64_t>(flags));
    payload["lock_strength"] = Value(static_cast<uint64_t>(stmt->lock_strength));
    payload["distinct_on"] = toExprList(stmt->distinct_on);

    payload["select_items"] = toSelectItems(stmt->items);
    if (stmt->from) {
        payload["from"] = toTableRef(stmt->from);
    }
    payload["joins"] = toJoins(stmt->joins);
    if (stmt->where) {
        // Preserve an explicit WHERE opcode marker for downstream plan/introspection tests.
        Instruction where_inst;
        where_inst.opcode = op(Opcode::SBLR3_WHERE_CLAUSE);
        where_inst.flags = 0;
        Value::Object where_payload;
        where_payload["predicate"] = Value(makeInstr(emitExpression(stmt->where)));
        where_inst.payload = Value(std::move(where_payload));
        payload["where"] = Value(makeInstr(std::move(where_inst)));
    }
    payload["group_by"] = toExprList(stmt->group_by);

    Value::List grouping_sets;
    for (const auto& group : stmt->grouping_sets) {
        grouping_sets.push_back(toExprList(group));
    }
    payload["grouping_sets"] = Value(std::move(grouping_sets));
    payload["grouping_type"] = Value(static_cast<uint64_t>(mapGroupingType(stmt->grouping_type)));

    if (stmt->having) {
        payload["having"] = Value(makeInstr(emitExpression(stmt->having)));
    }
    payload["order_by"] = toOrderBy(stmt->order_by);
    if (stmt->limit) {
        payload["limit"] = Value(makeInstr(emitExpression(stmt->limit)));
    }
    if (stmt->offset) {
        payload["offset"] = Value(makeInstr(emitExpression(stmt->offset)));
    }
    if (stmt->optimize_for_rows) {
        payload["optimize_for_rows"] = Value(makeInstr(emitExpression(stmt->optimize_for_rows)));
    }
    if (stmt->firebird_plan) {
        payload["plan"] = Value(makeInstr(emitExpression(stmt->firebird_plan)));
    }
    if (stmt->fetch_mode != parser::v3::FetchMode::NONE && stmt->fetch_row_count) {
        Value::Object fetch;
        fetch["mode"] = Value(static_cast<uint64_t>(stmt->fetch_mode));
        fetch["with_ties"] = Value(stmt->fetch_with_ties);
        fetch["row_count"] = Value(makeInstr(emitExpression(stmt->fetch_row_count)));
        payload["fetch"] = Value(std::move(fetch));
    }
    if (stmt->set_op != parser::v3::SetOpType::NONE && stmt->set_op_right) {
        Value::Object setop;
        setop["type"] = Value(static_cast<uint64_t>(stmt->set_op));
        setop["all"] = Value(stmt->set_op_all);
        setop["right"] = Value(makeInstr(emitSelect(stmt->set_op_right)));
        payload["set_op"] = Value(std::move(setop));
    }
    if (stmt->with) {
        Value::List ctes;
        for (const auto& cte : stmt->with->ctes) {
            Value::Object c;
            c["name"] = toIdent(cte.name);
            Value::List cols;
            for (auto id : cte.column_names) {
                cols.push_back(toIdent(id));
            }
            c["column_names"] = Value(std::move(cols));
            if (cte.query) {
                c["query"] = Value(makeInstr(emitStatement(cte.query)));
            }
            c["recursive"] = Value(cte.recursive || stmt->with->recursive);

            if (cte.has_search) {
                auto map_search_order = [](parser::v3::CTE::SearchOrder order) -> uint64_t {
                    switch (order) {
                        case parser::v3::CTE::SearchOrder::BREADTH_FIRST:
                            return 1;
                        case parser::v3::CTE::SearchOrder::DEPTH_FIRST:
                            return 2;
                        case parser::v3::CTE::SearchOrder::NONE:
                        default:
                            return 0;
                    }
                };
                Value::Object search;
                search["order"] = Value(map_search_order(cte.search_order));
                Value::List by_cols;
                for (auto id : cte.search_by_columns) {
                    by_cols.push_back(toIdent(id));
                }
                search["by_columns"] = Value(std::move(by_cols));
                search["set_column"] = toIdent(cte.search_sequence_column);
                c["search"] = Value(std::move(search));
            }

            if (cte.has_cycle) {
                Value::Object cycle;
                Value::List cycle_cols;
                for (auto id : cte.cycle_columns) {
                    cycle_cols.push_back(toIdent(id));
                }
                cycle["columns"] = Value(std::move(cycle_cols));
                cycle["set_column"] = toIdent(cte.cycle_mark_column);
                if (cte.has_cycle_mark_value && cte.cycle_mark_value) {
                    cycle["to_value"] = Value(makeInstr(emitExpression(cte.cycle_mark_value)));
                }
                if (cte.has_cycle_default_value && cte.cycle_mark_default) {
                    cycle["default_value"] = Value(makeInstr(emitExpression(cte.cycle_mark_default)));
                }
                cycle["path_column"] = toIdent(cte.cycle_path_column);
                c["cycle"] = Value(std::move(cycle));
            }

            ctes.push_back(Value(std::move(c)));
        }
        payload["with"] = Value(std::move(ctes));
    }

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitInsert(parser::v3::InsertStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_INSERT);
    inst.flags = 0;

    Value::Object payload;
    payload["target"] = toSchemaPath(stmt->table_path);
    if (stmt->has_alias) {
        payload["alias"] = toIdent(stmt->alias);
    }
    Value::List cols;
    for (auto id : stmt->columns) cols.push_back(toIdent(id));
    payload["columns"] = Value(std::move(cols));

    uint8_t source = 1;
    if (stmt->source == parser::v3::InsertStmt::Source::SELECT) source = 2;
    if (stmt->source == parser::v3::InsertStmt::Source::DEFAULT) source = 3;
    payload["source"] = Value(static_cast<uint64_t>(source));
    payload["overriding"] = Value(static_cast<uint64_t>(stmt->overriding));

    if (stmt->source == parser::v3::InsertStmt::Source::VALUES) {
        Value::List rows;
        for (const auto& row : stmt->values_rows) {
            rows.push_back(toExprList(row));
        }
        payload["values"] = Value(std::move(rows));
    }
    if (stmt->source == parser::v3::InsertStmt::Source::SELECT && stmt->select_source) {
        payload["select"] = Value(makeInstr(emitSelect(stmt->select_source)));
    }
    if (stmt->on_conflict) {
        Value::Object oc;
        Value::List target_cols;
        for (auto id : stmt->on_conflict->columns) target_cols.push_back(toIdent(id));
        oc["target_cols"] = Value(std::move(target_cols));
        oc["action"] = Value(static_cast<uint64_t>(stmt->on_conflict->action == parser::v3::ConflictAction::UPDATE ? 2 : 1));
        Value::List assignments;
        for (const auto& item : stmt->on_conflict->set_items) {
            Value::Object a;
            a["column"] = emitColumnRefValue(item.first);
            a["value"] = Value(makeInstr(emitExpression(item.second)));
            assignments.push_back(Value(std::move(a)));
        }
        oc["assignments"] = Value(std::move(assignments));
        if (stmt->on_conflict->where_action) {
            oc["where"] = Value(makeInstr(emitExpression(stmt->on_conflict->where_action)));
        }
        payload["on_conflict"] = Value(std::move(oc));
    }
    if (stmt->consistency_level != parser::v3::StringPool::INVALID_ID) {
        payload["consistency"] = toIdent(stmt->consistency_level);
    }
    if (stmt->serial_consistency_level != parser::v3::StringPool::INVALID_ID) {
        payload["serial_consistency"] = toIdent(stmt->serial_consistency_level);
    }
    if (stmt->conditional_if_exists) {
        payload["if_exists"] = Value(true);
    }
    if (stmt->conditional_if_not_exists) {
        payload["if_not_exists"] = Value(true);
    }
    if (stmt->conditional_if) {
        payload["if_condition"] = Value(makeInstr(emitExpression(stmt->conditional_if)));
    }

    Value::List returning;
    for (auto* item : stmt->returning) {
        if (item->item_type == parser::v3::SelectItem::Type::EXPRESSION && item->expr) {
            returning.push_back(Value(makeInstr(emitExpression(item->expr))));
        }
    }
    payload["returning"] = Value(std::move(returning));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitUpdate(parser::v3::UpdateStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_UPDATE);
    inst.flags = 0;

    Value::Object payload;
    payload["target"] = toSchemaPath(stmt->table_path);
    if (stmt->has_alias) payload["alias"] = toIdent(stmt->alias);

    Value::List assigns;
    for (const auto& item : stmt->set_items) {
        Value::Object a;
        a["column"] = emitColumnRefValue(item.first);
        a["value"] = Value(makeInstr(emitExpression(item.second)));
        assigns.push_back(Value(std::move(a)));
    }
    payload["set_items"] = Value(std::move(assigns));

    if (stmt->from) payload["from"] = toTableRef(stmt->from);
    payload["joins"] = toJoins(stmt->joins);
    if (stmt->where) payload["where"] = Value(makeInstr(emitExpression(stmt->where)));
    if (stmt->consistency_level != parser::v3::StringPool::INVALID_ID) {
        payload["consistency"] = toIdent(stmt->consistency_level);
    }
    if (stmt->serial_consistency_level != parser::v3::StringPool::INVALID_ID) {
        payload["serial_consistency"] = toIdent(stmt->serial_consistency_level);
    }
    if (stmt->conditional_if_exists) {
        payload["if_exists"] = Value(true);
    }
    if (stmt->conditional_if) {
        payload["if_condition"] = Value(makeInstr(emitExpression(stmt->conditional_if)));
    }

    Value::List returning;
    for (auto* item : stmt->returning) {
        if (item->item_type == parser::v3::SelectItem::Type::EXPRESSION && item->expr) {
            returning.push_back(Value(makeInstr(emitExpression(item->expr))));
        }
    }
    payload["returning"] = Value(std::move(returning));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDelete(parser::v3::DeleteStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_DELETE);
    inst.flags = 0;

    Value::Object payload;
    payload["target"] = toSchemaPath(stmt->table_path);
    if (stmt->has_alias) payload["alias"] = toIdent(stmt->alias);
    if (stmt->using_clause) payload["using"] = toTableRef(stmt->using_clause);
    payload["using_joins"] = toJoins(stmt->using_joins);
    if (stmt->where) payload["where"] = Value(makeInstr(emitExpression(stmt->where)));
    if (stmt->consistency_level != parser::v3::StringPool::INVALID_ID) {
        payload["consistency"] = toIdent(stmt->consistency_level);
    }
    if (stmt->serial_consistency_level != parser::v3::StringPool::INVALID_ID) {
        payload["serial_consistency"] = toIdent(stmt->serial_consistency_level);
    }
    if (stmt->conditional_if_exists) {
        payload["if_exists"] = Value(true);
    }
    if (stmt->conditional_if) {
        payload["if_condition"] = Value(makeInstr(emitExpression(stmt->conditional_if)));
    }

    Value::List returning;
    for (auto* item : stmt->returning) {
        if (item->item_type == parser::v3::SelectItem::Type::EXPRESSION && item->expr) {
            returning.push_back(Value(makeInstr(emitExpression(item->expr))));
        }
    }
    payload["returning"] = Value(std::move(returning));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitMerge(parser::v3::MergeStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_MERGE_START);
    inst.flags = 0;

    Value::Object payload;
    payload["target"] = toSchemaPath(stmt->target_table);
    if (stmt->target_alias != parser::v3::StringPool::INVALID_ID) {
        payload["target_alias"] = toIdent(stmt->target_alias);
    }
    if (stmt->source_query) {
        payload["source_query"] = Value(makeInstr(emitStatement(stmt->source_query)));
    } else if (!stmt->source_table.isEmpty()) {
        payload["source_table"] = toTableRefFromPath(stmt->source_table, stmt->source_alias);
    }
    if (stmt->source_alias != parser::v3::StringPool::INVALID_ID) {
        payload["source_alias"] = toIdent(stmt->source_alias);
    }
    if (stmt->on_condition) {
        payload["on"] = Value(makeInstr(emitExpression(stmt->on_condition)));
    }

    Value::List matched;
    for (const auto& action : stmt->when_matched) {
        Value::Object a;
        a["action"] = Value(static_cast<uint64_t>(action.is_delete ? 2 : 1));
        if (action.and_condition) {
            a["condition"] = Value(makeInstr(emitExpression(action.and_condition)));
        }
        Value::List assignments;
        for (const auto& item : action.assignments) {
            Value::Object as;
            as["column"] = emitColumnRefValue(item.first);
            as["value"] = Value(makeInstr(emitExpression(item.second)));
            assignments.push_back(Value(std::move(as)));
        }
        a["assignments"] = Value(std::move(assignments));
        matched.push_back(Value(std::move(a)));
    }
    payload["when_matched"] = Value(std::move(matched));

    Value::List not_matched;
    for (const auto& action : stmt->when_not_matched) {
        Value::Object a;
        a["action"] = Value(static_cast<uint64_t>(3));
        if (action.and_condition) {
            a["condition"] = Value(makeInstr(emitExpression(action.and_condition)));
        }
        Value::List cols;
        for (auto id : action.columns) cols.push_back(toIdent(id));
        a["insert_columns"] = Value(std::move(cols));
        a["insert_values"] = toExprList(action.values);
        not_matched.push_back(Value(std::move(a)));
    }
    payload["when_not_matched"] = Value(std::move(not_matched));

    Value::List not_matched_src;
    for (const auto& action : stmt->when_not_matched_by_source) {
        Value::Object a;
        a["action"] = Value(static_cast<uint64_t>(action.is_delete ? 2 : 1));
        if (action.and_condition) {
            a["condition"] = Value(makeInstr(emitExpression(action.and_condition)));
        }
        Value::List assignments;
        for (const auto& item : action.assignments) {
            Value::Object as;
            as["column"] = emitColumnRefValue(item.first);
            as["value"] = Value(makeInstr(emitExpression(item.second)));
            assignments.push_back(Value(std::move(as)));
        }
        a["assignments"] = Value(std::move(assignments));
        not_matched_src.push_back(Value(std::move(a)));
    }
    payload["when_not_matched_by_source"] = Value(std::move(not_matched_src));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitCopy(parser::v3::CopyStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_COPY);
    inst.flags = 0;

    Value::Object payload;
    auto makeStringLiteralInstr = [&](const std::string& text) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(text)}});
        return lit;
    };
    auto makeBoolLiteralInstr = [&](bool value) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_BOOLEAN);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(value)}});
        return lit;
    };
    auto makeIntLiteralInstr = [&](int64_t value) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_INT64);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(value)}});
        return lit;
    };
    auto addOption = [&](Value::List& options, const std::string& key, const Instruction& value) {
        Value::Object entry;
        entry["key"] = Value(key);
        entry["value"] = Value(makeInstr(value));
        options.push_back(Value(std::move(entry)));
    };

    payload["has_query"] = Value(stmt->query != nullptr);
    if (stmt->query) {
        payload["query"] = Value(makeInstr(emitSelect(stmt->query)));
    }
    if (!stmt->table_path.isEmpty()) {
        payload["target_table"] = toSchemaPath(stmt->table_path);
    }
    Value::List cols;
    for (auto id : stmt->columns) cols.push_back(toIdent(id));
    payload["columns"] = Value(std::move(cols));
    payload["direction"] = Value(static_cast<uint64_t>(stmt->direction == parser::v3::CopyStmt::Direction::FROM ? 1 : 2));
    payload["target_stdin"] = Value(stmt->target_is_stdin);
    payload["target_stdout"] = Value(stmt->target_is_stdout);
    payload["target_program"] = Value(stmt->target_is_program);
    if (!stmt->target_is_stdin && !stmt->target_is_stdout && stmt->target != parser::v3::StringPool::INVALID_ID) {
        payload["filename"] = Value(std::string(pool_.get(stmt->target)));
    }
    payload["format"] = Value(static_cast<uint64_t>(stmt->options.format_set ? static_cast<uint8_t>(stmt->options.format) + 1 : 0));

    Value::List options;
    if (stmt->options.delimiter_set) {
        addOption(options, "DELIMITER", makeStringLiteralInstr(std::string(pool_.get(stmt->options.delimiter))));
    }
    if (stmt->options.null_set) {
        addOption(options, "NULL", makeStringLiteralInstr(std::string(pool_.get(stmt->options.null_string))));
    }
    if (stmt->options.header_set) {
        addOption(options, "HEADER", makeBoolLiteralInstr(stmt->options.header));
    }
    if (stmt->options.quote_set) {
        addOption(options, "QUOTE", makeStringLiteralInstr(std::string(pool_.get(stmt->options.quote))));
    }
    if (stmt->options.escape_set) {
        addOption(options, "ESCAPE", makeStringLiteralInstr(std::string(pool_.get(stmt->options.escape))));
    }
    if (stmt->options.encoding_set) {
        addOption(options, "ENCODING", makeStringLiteralInstr(std::string(pool_.get(stmt->options.encoding))));
    }
    if (stmt->options.batch_size_set) {
        addOption(options, "BATCH_SIZE", makeIntLiteralInstr(stmt->options.batch_size));
    }
    if (stmt->options.max_errors_set) {
        addOption(options, "MAX_ERRORS", makeIntLiteralInstr(stmt->options.max_errors));
    }
    if (stmt->options.on_error_set) {
        std::string on_error = (stmt->options.on_error == parser::v3::CopyOptions::OnError::SKIP)
            ? "SKIP"
            : "ABORT";
        addOption(options, "ON_ERROR", makeStringLiteralInstr(on_error));
    }
    payload["options"] = Value(std::move(options));

    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDdlCreate(parser::v3::Statement* stmt) {
    auto makeStringLiteralInstr = [&](const std::string& text) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(text)}});
        return lit;
    };
    auto makeOptionKvPlaceholder = [&](uint64_t count,
                                       const std::string& key,
                                       const Instruction& value_instr) {
        return Value(Value::Object{
            {"count", Value(count)},
            {"key", Value(key)},
            {"value", Value(makeInstr(value_instr))},
        });
    };
    switch (stmt->kind()) {
        case parser::v3::ASTKind::CreateTableStmt: {
            auto* s = static_cast<parser::v3::CreateTableStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_TABLE);
            inst.flags = 0;
            Value::Object payload;
            uint64_t flags = 0;
            if (s->if_not_exists) flags |= 0x0001;
            if (s->unlogged) flags |= 0x0002;
            payload["flags"] = Value(flags);
            payload["temp_type"] = Value(uint64_t(static_cast<uint8_t>(s->temp_type)));
            payload["on_commit"] = Value(uint64_t(static_cast<uint8_t>(s->on_commit)));
            payload["path"] = toSchemaPath(s->table_path);
            Value::List cols;
            for (auto* col : s->columns) cols.push_back(emitColumnDef(col));
            payload["columns"] = Value(std::move(cols));
            Value::List constraints;
            for (auto* c : s->constraints) constraints.push_back(emitTableConstraint(c));
            payload["constraints"] = Value(std::move(constraints));
            Value::List inherits;
            for (const auto& p : s->inherits) inherits.push_back(toSchemaPath(p));
            payload["inherits"] = Value(std::move(inherits));
            if (s->has_tablespace) payload["tablespace"] = toSchemaPath(s->tablespace);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateIndexStmt: {
            auto* s = static_cast<parser::v3::CreateIndexStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_INDEX);
            inst.flags = 0;
            Value::Object payload;
            uint64_t flags = 0;
            if (s->if_not_exists) flags |= 0x0001;
            if (s->unique) flags |= 0x0002;
            payload["flags"] = Value(flags);
            payload["index_path"] = toSchemaPath(parser::v3::SchemaPath(parser::v3::PathType::UNQUALIFIED, {s->index_name}));
            payload["table"] = toSchemaPath(s->table_path);
            Value::List keys;
            for (const auto& key : s->columns) {
                Value::Object k;
                k["kind"] = Value(uint64_t(key.expr ? 2 : 1));
                if (key.expr) {
                    k["name_or_expr"] = Value(makeInstr(emitExpression(key.expr)));
                } else {
                    Instruction colref;
                    colref.opcode = op(Opcode::SBLR3_COLUMN_REF);
                    colref.flags = 0;
                    colref.payload = emitColumnRefValue(key.column);
                    k["name_or_expr"] = Value(makeInstr(colref));
                }
                k["order"] = Value(uint64_t(key.ascending ? 0 : 1));
                if (key.nulls_first) k["nulls"] = Value(uint64_t(1));
                else if (key.nulls_last) k["nulls"] = Value(uint64_t(2));
                if (key.opclass != parser::v3::StringPool::INVALID_ID) {
                    k["opclass"] = toIdent(key.opclass);
                }
                keys.push_back(Value(std::move(k)));
            }
            payload["keys"] = Value(std::move(keys));
            Value::List include;
            for (auto id : s->include_columns) include.push_back(toIdent(id));
            payload["include"] = Value(std::move(include));
            if (s->where_clause) payload["predicate"] = Value(makeInstr(emitExpression(s->where_clause)));
            payload["index_type"] = Value(indexTypeName(s->index_type));
            if (!s->option_assignments.empty()) {
                payload["options"] = makeOptionKvFromAssignments(pool_,
                                                                 s->option_assignments);
            } else {
                uint64_t option_count = 0;
                std::string option_key;
                Instruction option_value = emitLiteral(nullptr);
                if (s->options.bloom_filter_set) {
                    option_count++;
                    option_key = "BLOOM_FILTER";
                    option_value = makeBoolLiteralInstruction(s->options.bloom_filter_enabled);
                }
                if (s->options.bloom_fpr_set) {
                    if (option_count == 0) {
                        option_key = "BLOOM_FPR";
                        option_value = makeDoubleLiteralInstruction(s->options.bloom_fpr);
                    }
                    option_count++;
                }
                payload["options"] = makeOptionKvPlaceholder(option_count, option_key, option_value);
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateViewStmt: {
            auto* s = static_cast<parser::v3::CreateViewStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_VIEW);
            inst.flags = 0;
            Value::Object payload;
            uint64_t flags = 0;
            if (s->if_not_exists) flags |= 0x0001;
            if (s->or_replace) flags |= 0x0002;
            if (s->temporary) flags |= 0x0004;
            if (s->materialized) flags |= 0x0008;
            if (s->with_check_option) flags |= 0x0010;
            payload["flags"] = Value(flags);
            payload["path"] = toSchemaPath(s->view_path);
            Value::List cols;
            for (auto id : s->column_names) cols.push_back(toIdent(id));
            payload["columns"] = Value(std::move(cols));
            if (s->query) payload["query"] = Value(makeInstr(emitStatement(s->query)));
            std::string rendered = renderSimpleSelectDefinition(pool_, s->query);
            if (!rendered.empty()) {
                payload["definition"] = Value(rendered);
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateSequenceStmt: {
            auto* s = static_cast<parser::v3::CreateSequenceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_SEQUENCE);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["path"] = toSchemaPath(s->sequence_path);
            if (s->start_with) payload["start"] = Value(int64_t(*s->start_with));
            if (s->increment_by) payload["increment"] = Value(int64_t(*s->increment_by));
            if (s->min_value) payload["min_value"] = Value(int64_t(*s->min_value));
            if (s->max_value) payload["max_value"] = Value(int64_t(*s->max_value));
            if (s->cache) payload["cache"] = Value(uint64_t(*s->cache));
            if (s->cycle) payload["cycle"] = Value(true);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateSchemaStmt: {
            auto* s = static_cast<parser::v3::CreateSchemaStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_SCHEMA);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["path"] = toSchemaPath(s->schema_path);
            if (s->has_owner) payload["owner"] = toIdent(s->owner);
            payload["path_list"] = Value(Value::List{});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateDatabaseStmt: {
            auto* s = static_cast<parser::v3::CreateDatabaseStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_DATABASE);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["name"] = toIdent(s->database_path.objectName());
            payload["encrypted"] = Value(false);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateTablespaceStmt: {
            auto* s = static_cast<parser::v3::CreateTablespaceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_TABLESPACE);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(0));
            payload["name"] = toIdent(s->tablespace_name);
            payload["location"] = Value(std::string(s->location));
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateFunctionStmt: {
            auto* s = static_cast<parser::v3::CreateFunctionStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_FUNCTION_STMT);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->function_path.objectName());
            Value::List params;
            for (const auto& p : s->params) {
                Value::Object param;
                param["name"] = toIdent(p.name);
                param["type"] = Value(buildTypeSpec(p.type));
                param["mode"] = Value(uint64_t(static_cast<uint8_t>(p.mode)));
                if (p.has_default && p.default_value) {
                    param["default_expr"] = Value(makeInstr(emitExpression(p.default_value)));
                }
                params.push_back(Value(std::move(param)));
            }
            payload["params"] = Value(std::move(params));
            payload["return_type"] = Value(buildTypeSpec(s->return_type));
            payload["language"] = Value(std::string("SQL"));
            if (s->body != parser::v3::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->body));
                payload["body"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateProcedureStmt: {
            auto* s = static_cast<parser::v3::CreateProcedureStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_PROCEDURE_STMT);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->procedure_path.objectName());
            Value::List params;
            for (const auto& p : s->params) {
                Value::Object param;
                param["name"] = toIdent(p.name);
                param["type"] = Value(buildTypeSpec(p.type));
                param["mode"] = Value(uint64_t(static_cast<uint8_t>(p.mode)));
                if (p.has_default && p.default_value) {
                    param["default_expr"] = Value(makeInstr(emitExpression(p.default_value)));
                }
                params.push_back(Value(std::move(param)));
            }
            payload["params"] = Value(std::move(params));
            payload["language"] = Value(std::string("SQL"));
            if (s->body != parser::v3::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->body));
                payload["body"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateTriggerStmt: {
            auto* s = static_cast<parser::v3::CreateTriggerStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_TRIGGER);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->trigger_name);
            payload["table"] = toSchemaPath(s->table_path);
            payload["timing"] = Value(uint64_t(static_cast<uint8_t>(s->timing)));
            payload["event_mask"] = Value(uint64_t(s->event_mask));
            payload["for_each_row"] = Value(s->granularity == parser::v3::TriggerGranularity::FOR_EACH_ROW);
            payload["is_database_trigger"] = Value(s->is_database_trigger);
            if (s->has_sql_security) {
                payload["sql_security"] = Value(uint64_t(static_cast<uint8_t>(s->sql_security)));
            }
            if (s->body != parser::v3::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->body));
                payload["body"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreatePackageStmt: {
            auto* s = static_cast<parser::v3::CreatePackageStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_PACKAGE_STMT);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->package_path.objectName());
            if (s->header != parser::v3::StringPool::INVALID_ID) {
                std::string header(pool_.get(s->header));
                payload["spec"] = Value(Value::Bytes(header.begin(), header.end()));
            }
            if (s->body != parser::v3::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->body));
                payload["body"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateUserStmt: {
            auto* s = static_cast<parser::v3::CreateUserStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_USER);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->user_name);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateRoleStmt: {
            auto* s = static_cast<parser::v3::CreateRoleStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_ROLE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->role_name);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateGroupStmt: {
            auto* s = static_cast<parser::v3::CreateGroupStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_GROUP);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->group_name);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreatePolicyStmt: {
            auto* s = static_cast<parser::v3::CreatePolicyStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_POLICY);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->policy_name);
            payload["table"] = toSchemaPath(s->table_path);
            payload["event_mask"] = Value(uint64_t(static_cast<uint8_t>(s->policy_type)));
            payload["is_permissive"] = Value(s->is_permissive);
            Value::List roles;
            for (auto id : s->roles) {
                roles.push_back(toIdent(id));
            }
            payload["roles"] = Value(std::move(roles));
            if (s->using_expr) payload["using_expr"] = Value(makeInstr(emitExpression(s->using_expr)));
            if (s->with_check_expr) payload["check_expr"] = Value(makeInstr(emitExpression(s->with_check_expr)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateForeignServerStmt: {
            auto* s = static_cast<parser::v3::CreateForeignServerStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_FOREIGN_SERVER);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->server_name);
            if (s->has_server_type) {
                payload["type"] = Value(std::string(s->server_type));
            } else {
                payload["type"] = toIdent(s->fdw_name);
            }
            if (s->has_server_version) {
                payload["host"] = Value(std::string(s->server_version));
            } else {
                payload["host"] = Value(std::string());
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateForeignDataWrapperStmt: {
            auto* s = static_cast<parser::v3::CreateForeignDataWrapperStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_FOREIGN_DATA_WRAPPER);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->wrapper_name);
            if (s->has_handler && s->handler_name != parser::v3::StringPool::INVALID_ID) {
                payload["handler"] = toIdent(s->handler_name);
            }
            if (s->has_validator && s->validator_name != parser::v3::StringPool::INVALID_ID) {
                payload["validator"] = toIdent(s->validator_name);
            }
            uint64_t opt_count = s->options.size();
            std::string opt_key;
            Instruction opt_value = emitLiteral(nullptr);
            if (!s->options.empty()) {
                const auto& opt = s->options.front();
                opt_key = opt.key;
                opt_value = makeStringLiteralInstr(opt.value);
            }
            payload["options"] = makeOptionKvPlaceholder(opt_count, opt_key, opt_value);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateForeignTableStmt: {
            auto* s = static_cast<parser::v3::CreateForeignTableStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_FOREIGN_TABLE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toSchemaPath(s->table_path);
            payload["server"] = toIdent(s->server_name);
            Value::List cols;
            for (const auto& c : s->columns) {
                parser::v3::ColumnDef def;
                def.name = c.name;
                def.type = c.type;
                cols.push_back(emitColumnDef(&def));
            }
            payload["columns"] = Value(std::move(cols));
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateUserMappingStmt: {
            auto* s = static_cast<parser::v3::CreateUserMappingStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_USER_MAPPING);
            inst.flags = 0;
            Value::Object payload;
            payload["server"] = toIdent(s->server_name);
            if (s->target == parser::v3::UserMappingTarget::PUBLIC_ROLE) {
                payload["user"] = Value(std::string("PUBLIC"));
            } else {
                payload["user"] = toIdent(s->user_name);
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateSynonymStmt: {
            auto* s = static_cast<parser::v3::CreateSynonymStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_SYNONYM);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toSchemaPath(s->synonym_path);
            payload["target"] = toSchemaPath(s->target_path);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateUdrStmt: {
            auto* s = static_cast<parser::v3::CreateUdrStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_UDR);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->udr_path.objectName());
            payload["library_path"] = Value(std::string(s->library_path));
            payload["entry_point"] = Value(std::string(s->entry_point));
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateJobStmt: {
            auto* s = static_cast<parser::v3::CreateJobStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_JOB);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->job_name);
            if (s->schedule_kind == parser::v3::JobScheduleKind::CRON) {
                payload["schedule"] = Value(std::string(pool_.get(s->cron_expression)));
            } else if (s->schedule_kind == parser::v3::JobScheduleKind::AT) {
                payload["schedule"] = Value(std::string(pool_.get(s->at_timestamp)));
            } else {
                payload["schedule"] = Value(std::to_string(s->interval_seconds));
            }
            if (s->job_type == parser::v3::JobType::SQL && s->job_sql != parser::v3::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->job_sql));
                payload["command"] = Value(Value::Bytes(body.begin(), body.end()));
            } else if (s->job_type == parser::v3::JobType::PROCEDURE &&
                       s->procedure_name != parser::v3::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->procedure_name));
                payload["command"] = Value(Value::Bytes(body.begin(), body.end()));
            } else if (s->job_type == parser::v3::JobType::EXTERNAL &&
                       s->external_command != parser::v3::StringPool::INVALID_ID) {
                std::string body(pool_.get(s->external_command));
                payload["command"] = Value(Value::Bytes(body.begin(), body.end()));
            }
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateExceptionStmt: {
            auto* s = static_cast<parser::v3::CreateExceptionStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_EXCEPTION_STMT);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->exception_path.objectName());
            if (s->message != parser::v3::StringPool::INVALID_ID) {
                payload["message"] = Value(std::string(pool_.get(s->message)));
            } else {
                payload["message"] = Value(std::string());
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateDomainStmt: {
            auto* s = static_cast<parser::v3::CreateDomainStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_DOMAIN);
            inst.flags = 0;
            Value::Object payload;
            payload["flags"] = Value(uint64_t(s->if_not_exists ? 0x0001 : 0));
            payload["path"] = toSchemaPath(s->domain_path);
            payload["name"] = toIdent(s->domain_path.objectName());
            payload["type"] = Value(buildTypeSpec(s->base_type));
            payload["domain_kind"] = Value(uint64_t(static_cast<uint8_t>(s->domain_kind)));
            if (s->has_collation && !s->collation_name.empty()) {
                payload["collation"] = Value(s->collation_name);
            }
            if (s->has_dialect && !s->dialect_tag.empty()) {
                payload["dialect_tag"] = Value(s->dialect_tag);
            }
            if (s->has_compat && !s->compat_name.empty()) {
                payload["compat_name"] = Value(s->compat_name);
            }
            payload["enum_wrap"] = Value(s->enum_wrap);
            payload["has_inherits"] = Value(s->has_inherits);
            if (s->has_inherits && !s->parent_domain_path.isEmpty()) {
                payload["parent_path"] = toSchemaPath(s->parent_domain_path);
            }
            Value::List constraints;
            for (const auto& c : s->constraints) {
                Value::Object cobj;
                cobj["type"] = Value(uint64_t(static_cast<uint8_t>(c.type)));
                if (c.name != parser::v3::StringPool::INVALID_ID) {
                    cobj["name"] = toIdent(c.name);
                }
                if (!c.expression.empty()) {
                    cobj["expression"] = Value(c.expression);
                }
                constraints.push_back(Value(std::move(cobj)));
            }
            payload["constraints"] = Value(std::move(constraints));

            payload["has_integrity"] = Value(s->has_integrity);
            if (s->has_integrity) {
                Value::Object integrity;
                integrity["has_uniqueness"] = Value(s->integrity.has_uniqueness);
                integrity["uniqueness"] = Value(s->integrity.uniqueness);
                integrity["normalization_enabled"] = Value(s->integrity.normalization_enabled);
                if (!s->integrity.normalization_function.empty()) {
                    integrity["normalization_function"] = Value(s->integrity.normalization_function);
                }
                payload["integrity"] = Value(std::move(integrity));
            }

            payload["has_security"] = Value(s->has_security);
            if (s->has_security) {
                Value::Object security;
                security["has_masking"] = Value(s->security.has_masking);
                if (!s->security.masking.empty()) {
                    security["masking"] = Value(s->security.masking);
                }
                security["has_mask_pattern"] = Value(s->security.has_mask_pattern);
                if (!s->security.mask_pattern.empty()) {
                    security["mask_pattern"] = Value(s->security.mask_pattern);
                }
                security["has_encryption"] = Value(s->security.has_encryption);
                if (!s->security.encryption.empty()) {
                    security["encryption"] = Value(s->security.encryption);
                }
                security["has_audit_access"] = Value(s->security.has_audit_access);
                security["audit_access"] = Value(s->security.audit_access);
                security["has_required_privilege"] = Value(s->security.has_required_privilege);
                if (!s->security.required_privilege.empty()) {
                    security["required_privilege"] = Value(s->security.required_privilege);
                }
                payload["security"] = Value(std::move(security));
            }

            payload["has_validation"] = Value(s->has_validation);
            if (s->has_validation) {
                Value::Object validation;
                validation["has_function"] = Value(s->validation.has_function);
                if (!s->validation.function.empty()) {
                    validation["function"] = Value(s->validation.function);
                }
                validation["has_error_message"] = Value(s->validation.has_error_message);
                if (!s->validation.error_message.empty()) {
                    validation["error_message"] = Value(s->validation.error_message);
                }
                payload["validation"] = Value(std::move(validation));
            }

            payload["has_quality"] = Value(s->has_quality);
            if (s->has_quality) {
                Value::Object quality;
                quality["has_parse_function"] = Value(s->quality.has_parse_function);
                if (!s->quality.parse_function.empty()) {
                    quality["parse_function"] = Value(s->quality.parse_function);
                }
                quality["has_standardize_function"] = Value(s->quality.has_standardize_function);
                if (!s->quality.standardize_function.empty()) {
                    quality["standardize_function"] = Value(s->quality.standardize_function);
                }
                quality["has_enrich_function"] = Value(s->quality.has_enrich_function);
                if (!s->quality.enrich_function.empty()) {
                    quality["enrich_function"] = Value(s->quality.enrich_function);
                }
                payload["quality"] = Value(std::move(quality));
            }

            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CreateTypeStmt: {
            auto* s = static_cast<parser::v3::CreateTypeStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_CREATE_TYPE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->type_path.objectName());
            payload["type"] = Value(TypeSpec{});
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.flags = 0;
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDdlAlter(parser::v3::Statement* stmt) {
    auto encodePayload = [&](const scratchbird::sblr::v3::SchemaDef& schema,
                             const scratchbird::sblr::v3::Value::Object& obj) -> Value::Bytes {
        Buffer out;
        DecodeError err;
        if (!scratchbird::sblr::v3::encodePayloadBySchema(schema, Value(obj), out, err)) {
            fail(err.message);
            return Value::Bytes{};
        }
        return out;
    };
    auto makeStringLiteralInstr = [&](const std::string& text) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(text)}});
        return lit;
    };
    auto makeOptionKvPlaceholder = [&](uint64_t count,
                                       const std::string& key,
                                       const Instruction& value_instr) {
        return Value(Value::Object{
            {"count", Value(count)},
            {"key", Value(key)},
            {"value", Value(makeInstr(value_instr))},
        });
    };
    switch (stmt->kind()) {
        case parser::v3::ASTKind::AlterTableStmt: {
            auto* s = static_cast<parser::v3::AlterTableStmt*>(stmt);
            if (s->action == parser::v3::AlterTableAction::SET_TABLESPACE) {
                Instruction inst;
                inst.opcode = op(Opcode::SBLR3_ALTER_TABLE_SET_TABLESPACE);
                inst.flags = 0;
                Value::Object payload;
                payload["table"] = toSchemaPath(s->table_path);
                payload["tablespace"] = toSchemaPath(s->tablespace);
                payload["options"] = Value(Value::Object{
                    {"count", Value(uint64_t(0))},
                    {"key", Value(std::string())},
                    {"value", Value(makeInstr(emitLiteral(nullptr)))}});
                inst.payload = Value(std::move(payload));
                return inst;
            }
            if (s->action == parser::v3::AlterTableAction::RENAME_TABLE) {
                Instruction inst;
                inst.opcode = op(Opcode::SBLR3_RENAME_OBJECT);
                inst.flags = 0;
                Value::Object payload;
                payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(parser::v3::DdlObjectType::TABLE)));
                payload["object_path"] = toSchemaPath(s->table_path);
                payload["new_name"] = toIdent(s->new_name);
                inst.payload = Value(std::move(payload));
                return inst;
            }
            if (s->action == parser::v3::AlterTableAction::RENAME_COLUMN) {
                Instruction inst;
                inst.opcode = op(Opcode::SBLR3_RENAME_OBJECT);
                inst.flags = 0;
                Value::Object payload;
                payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(parser::v3::DdlObjectType::COLUMN)));
                parser::v3::SchemaPath column_path = s->table_path;
                column_path.components.push_back(s->column_name);
                payload["object_path"] = toSchemaPath(column_path);
                payload["new_name"] = toIdent(s->new_name);
                inst.payload = Value(std::move(payload));
                return inst;
            }
            if (s->action == parser::v3::AlterTableAction::RENAME_CONSTRAINT) {
                Instruction inst;
                inst.opcode = op(Opcode::SBLR3_RENAME_OBJECT);
                inst.flags = 0;
                Value::Object payload;
                payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(parser::v3::DdlObjectType::CONSTRAINT)));
                parser::v3::SchemaPath constraint_path = s->table_path;
                constraint_path.components.push_back(s->constraint_name);
                payload["object_path"] = toSchemaPath(constraint_path);
                payload["new_name"] = toIdent(s->new_name);
                inst.payload = Value(std::move(payload));
                return inst;
            }
            if (s->action == parser::v3::AlterTableAction::SET_SCHEMA) {
                Instruction inst;
                inst.opcode = op(Opcode::SBLR3_MOVE_OBJECT);
                inst.flags = 0;
                Value::Object payload;
                payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(parser::v3::DdlObjectType::TABLE)));
                payload["object_path"] = toSchemaPath(s->table_path);
                if (!s->target_schema.components.empty()) {
                    payload["new_name"] = toIdent(s->target_schema.components.back());
                } else {
                    payload["new_name"] = Value(std::string());
                }
                inst.payload = Value(std::move(payload));
                return inst;
            }
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_TABLE);
            inst.flags = 0;
            Value::Object payload;
            payload["table"] = toSchemaPath(s->table_path);
            payload["if_exists"] = Value(s->if_exists);
            payload["only"] = Value(s->only);
            uint8_t action_code = 0;
            switch (s->action) {
                case parser::v3::AlterTableAction::ADD_COLUMN: action_code = 1; break;
                case parser::v3::AlterTableAction::ADD_CONSTRAINT: action_code = 2; break;
                case parser::v3::AlterTableAction::DROP_COLUMN: action_code = 3; break;
                case parser::v3::AlterTableAction::DROP_CONSTRAINT: action_code = 4; break;
                case parser::v3::AlterTableAction::ALTER_COLUMN: action_code = 5; break;
                case parser::v3::AlterTableAction::ALTER_COLUMN_POSITION: action_code = 6; break;
                case parser::v3::AlterTableAction::ALTER_COLUMN_SET_DEFAULT: action_code = 7; break;
                case parser::v3::AlterTableAction::ALTER_COLUMN_DROP_DEFAULT: action_code = 8; break;
                case parser::v3::AlterTableAction::ALTER_COLUMN_SET_NOT_NULL: action_code = 9; break;
                case parser::v3::AlterTableAction::ALTER_COLUMN_DROP_NOT_NULL: action_code = 10; break;
                case parser::v3::AlterTableAction::RENAME_TABLE: action_code = 11; break;
                case parser::v3::AlterTableAction::RENAME_CONSTRAINT: action_code = 12; break;
                case parser::v3::AlterTableAction::SET_SCHEMA: action_code = 13; break;
                case parser::v3::AlterTableAction::RENAME_COLUMN: action_code = 15; break;
                case parser::v3::AlterTableAction::SET_STATISTICS: action_code = 16; break;
                case parser::v3::AlterTableAction::SET_STORAGE: action_code = 17; break;
                case parser::v3::AlterTableAction::INHERIT: action_code = 18; break;
                case parser::v3::AlterTableAction::NO_INHERIT: action_code = 19; break;
                case parser::v3::AlterTableAction::ENABLE_TRIGGER: action_code = 20; break;
                case parser::v3::AlterTableAction::DISABLE_TRIGGER: action_code = 21; break;
                case parser::v3::AlterTableAction::ENABLE_RLS: action_code = 22; break;
                case parser::v3::AlterTableAction::DISABLE_RLS: action_code = 23; break;
                case parser::v3::AlterTableAction::FORCE_RLS: action_code = 24; break;
                case parser::v3::AlterTableAction::NO_FORCE_RLS: action_code = 25; break;
                case parser::v3::AlterTableAction::ATTACH_PARTITION: action_code = 26; break;
                case parser::v3::AlterTableAction::DETACH_PARTITION: action_code = 27; break;
                case parser::v3::AlterTableAction::VALIDATE_CONSTRAINT: action_code = 28; break;
                default:
                    action_code = 0;
                    break;
            }
            payload["action"] = Value(uint64_t(action_code));
            Value::Bytes action_payload;
            switch (s->action) {
                case parser::v3::AlterTableAction::ADD_COLUMN: {
                    if (s->column) {
                        auto col = emitColumnDef(s->column);
                        if (auto obj = std::get_if<Value::Object>(&col.data)) {
                            if (const auto* schema = scratchbird::sblr::v3::lookupSchema("COLUMN_DEF")) {
                                action_payload = encodePayload(*schema, *obj);
                            }
                        }
                    }
                    break;
                }
                case parser::v3::AlterTableAction::DROP_COLUMN: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DROP_COLUMN", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"cascade", scratchbird::sblr::v3::FieldType::BOOL, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    obj["cascade"] = Value(s->cascade);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ALTER_COLUMN: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_ALTER_COLUMN_TYPE", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"type", scratchbird::sblr::v3::FieldType::TYPE_SPEC, ""},
                        scratchbird::sblr::v3::FieldDef{"charset", scratchbird::sblr::v3::FieldType::OPT, "ident"},
                        scratchbird::sblr::v3::FieldDef{"collation", scratchbird::sblr::v3::FieldType::OPT, "ident"},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    if (s->column) {
                        obj["type"] = Value(buildTypeSpec(s->column->type));
                        if (s->column->charset != parser::v3::StringPool::INVALID_ID) {
                            obj["charset"] = toIdent(s->column->charset);
                        }
                        for (const auto& c : s->column->constraints) {
                            if (c.type == parser::v3::ConstraintType::COLLATE &&
                                c.collation != parser::v3::StringPool::INVALID_ID) {
                                obj["collation"] = toIdent(c.collation);
                                break;
                            }
                        }
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ALTER_COLUMN_POSITION: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_ALTER_COLUMN_POSITION", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"position_1_based", scratchbird::sblr::v3::FieldType::U32, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    obj["position_1_based"] = Value(uint64_t(s->position_1_based));
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ALTER_COLUMN_SET_DEFAULT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_DEFAULT", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"default", scratchbird::sblr::v3::FieldType::EXPR, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    if (s->default_expr) {
                        obj["default"] = Value(makeInstr(emitExpression(s->default_expr)));
                    } else {
                        obj["default"] = Value(makeInstr(emitLiteral(nullptr)));
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ALTER_COLUMN_DROP_DEFAULT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DROP_DEFAULT", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ALTER_COLUMN_SET_NOT_NULL: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_NOT_NULL", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ALTER_COLUMN_DROP_NOT_NULL: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DROP_NOT_NULL", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::RENAME_COLUMN: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_RENAME_COLUMN", {
                        scratchbird::sblr::v3::FieldDef{"old_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"new_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["old_name"] = toIdent(s->column_name);
                    obj["new_name"] = toIdent(s->new_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ADD_CONSTRAINT: {
                    if (s->constraint) {
                        auto cons = emitTableConstraint(s->constraint);
                        if (auto obj = std::get_if<Value::Object>(&cons.data)) {
                            if (const auto* schema = scratchbird::sblr::v3::lookupSchema("TABLE_CONSTRAINT")) {
                                action_payload = encodePayload(*schema, *obj);
                            }
                        }
                    }
                    break;
                }
                case parser::v3::AlterTableAction::DROP_CONSTRAINT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DROP_CONSTRAINT", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"cascade", scratchbird::sblr::v3::FieldType::BOOL, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->constraint_name);
                    obj["cascade"] = Value(s->cascade);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::RENAME_CONSTRAINT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_RENAME_CONSTRAINT", {
                        scratchbird::sblr::v3::FieldDef{"old_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"new_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["old_name"] = toIdent(s->constraint_name);
                    obj["new_name"] = toIdent(s->new_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::RENAME_TABLE: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_RENAME_TABLE", {
                        scratchbird::sblr::v3::FieldDef{"new_name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["new_name"] = toIdent(s->new_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::SET_SCHEMA: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_SCHEMA", {
                        scratchbird::sblr::v3::FieldDef{"schema", scratchbird::sblr::v3::FieldType::SCHEMA_PATH, ""},
                    }};
                    Value::Object obj;
                    obj["schema"] = toSchemaPath(s->target_schema);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::SET_STATISTICS: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_STATISTICS", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"target", scratchbird::sblr::v3::FieldType::I32, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    obj["target"] = Value(int64_t(s->statistics_target));
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::SET_STORAGE: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_SET_STORAGE", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                        scratchbird::sblr::v3::FieldDef{"storage", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->column_name);
                    obj["storage"] = toIdent(s->storage_type);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::INHERIT:
                case parser::v3::AlterTableAction::NO_INHERIT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_INHERIT", {
                        scratchbird::sblr::v3::FieldDef{"parent", scratchbird::sblr::v3::FieldType::OPT, "schema_path"},
                    }};
                    Value::Object obj;
                    if (s->has_inherit_parent) {
                        obj["parent"] = toSchemaPath(s->inherit_parent);
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ENABLE_TRIGGER:
                case parser::v3::AlterTableAction::DISABLE_TRIGGER: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_TRIGGER_TOGGLE", {
                        scratchbird::sblr::v3::FieldDef{"trigger_all", scratchbird::sblr::v3::FieldType::BOOL, ""},
                        scratchbird::sblr::v3::FieldDef{"trigger_name", scratchbird::sblr::v3::FieldType::OPT, "ident"},
                    }};
                    Value::Object obj;
                    obj["trigger_all"] = Value(s->trigger_all);
                    if (!s->trigger_all && s->trigger_name != parser::v3::StringPool::INVALID_ID) {
                        obj["trigger_name"] = toIdent(s->trigger_name);
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::ENABLE_RLS:
                case parser::v3::AlterTableAction::DISABLE_RLS:
                case parser::v3::AlterTableAction::FORCE_RLS:
                case parser::v3::AlterTableAction::NO_FORCE_RLS: {
                    action_payload = Value::Bytes{};
                    break;
                }
                case parser::v3::AlterTableAction::ATTACH_PARTITION: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_ATTACH_PARTITION", {
                        scratchbird::sblr::v3::FieldDef{"partition", scratchbird::sblr::v3::FieldType::SCHEMA_PATH, ""},
                        scratchbird::sblr::v3::FieldDef{"bounds", scratchbird::sblr::v3::FieldType::OPT, "string"},
                    }};
                    Value::Object obj;
                    obj["partition"] = toSchemaPath(s->partition_path);
                    if (s->has_partition_bounds) {
                        obj["bounds"] = Value(std::string(pool_.get(s->partition_bounds)));
                    }
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::DETACH_PARTITION: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_DETACH_PARTITION", {
                        scratchbird::sblr::v3::FieldDef{"partition", scratchbird::sblr::v3::FieldType::SCHEMA_PATH, ""},
                    }};
                    Value::Object obj;
                    obj["partition"] = toSchemaPath(s->partition_path);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                case parser::v3::AlterTableAction::VALIDATE_CONSTRAINT: {
                    scratchbird::sblr::v3::SchemaDef schema{"ALTER_TABLE_VALIDATE_CONSTRAINT", {
                        scratchbird::sblr::v3::FieldDef{"name", scratchbird::sblr::v3::FieldType::IDENT, ""},
                    }};
                    Value::Object obj;
                    obj["name"] = toIdent(s->constraint_name);
                    action_payload = encodePayload(schema, obj);
                    break;
                }
                default:
                    break;
            }
            payload["payload"] = Value(std::move(action_payload));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterSequenceStmt: {
            auto* s = static_cast<parser::v3::AlterSequenceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_SEQUENCE);
            inst.flags = 0;
            Value::Object payload;
            payload["path"] = toSchemaPath(s->sequence_path);
            if (s->restart_with) payload["start"] = Value(int64_t(*s->restart_with));
            if (s->increment_by) payload["increment"] = Value(int64_t(*s->increment_by));
            if (s->min_value) payload["min_value"] = Value(int64_t(*s->min_value));
            if (s->max_value) payload["max_value"] = Value(int64_t(*s->max_value));
            if (s->cycle) payload["cycle"] = Value(*s->cycle);
            if (s->cache) payload["cache"] = Value(uint64_t(*s->cache));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterIndexStmt: {
            auto* s = static_cast<parser::v3::AlterIndexStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_INDEX);
            inst.flags = 0;
            Value::Object payload;
            payload["index"] = toSchemaPath(s->index_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            payload["defaults_scope"] = Value(s->defaults_scope);
            if (s->defaults_scope) {
                payload["defaults_index_type"] = Value(indexTypeName(s->defaults_index_type));
            }
            if (s->has_target_filespace) {
                payload["target_filespace"] = toSchemaPath(s->target_filespace);
            }
            payload["mode"] = Value(uint64_t(static_cast<uint8_t>(s->mode)));

            if (!s->option_assignments.empty()) {
                payload["options"] = makeOptionKvFromAssignments(pool_,
                                                                 s->option_assignments);
            } else if (!s->reset_options.empty()) {
                payload["options"] = makeOptionKvFromResetList(pool_,
                                                               s->reset_options,
                                                               emitLiteral(nullptr));
            } else {
                uint64_t option_count = 0;
                std::string option_key;
                Instruction option_value = emitLiteral(nullptr);
                if (s->options.bloom_filter_set) {
                    option_count++;
                    option_key = "BLOOM_FILTER";
                    option_value = makeBoolLiteralInstruction(s->options.bloom_filter_enabled);
                }
                if (s->options.bloom_fpr_set) {
                    if (option_count == 0) {
                        option_key = "BLOOM_FPR";
                        option_value = makeDoubleLiteralInstruction(s->options.bloom_fpr);
                    }
                    option_count++;
                }
                payload["options"] = makeOptionKvPlaceholder(option_count, option_key, option_value);
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterSchemaStmt: {
            auto* s = static_cast<parser::v3::AlterSchemaStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_SCHEMA);
            inst.flags = 0;
            Value::Object payload;
            payload["schema"] = toSchemaPath(s->schema_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            if (s->new_name != parser::v3::StringPool::INVALID_ID) payload["new_name"] = toIdent(s->new_name);
            if (s->owner != parser::v3::StringPool::INVALID_ID) payload["owner"] = toIdent(s->owner);
            if (!s->new_path.components.empty()) payload["new_path"] = toSchemaPath(s->new_path);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterDatabaseStmt: {
            auto* s = static_cast<parser::v3::AlterDatabaseStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_DATABASE);
            inst.flags = 0;
            Value::Object payload;
            payload["database"] = toSchemaPath(s->database_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            if (s->new_name != parser::v3::StringPool::INVALID_ID) payload["new_name"] = toIdent(s->new_name);
            if (s->owner != parser::v3::StringPool::INVALID_ID) payload["owner"] = toIdent(s->owner);
            if (s->alias != parser::v3::StringPool::INVALID_ID) payload["alias"] = toIdent(s->alias);
            uint64_t opt_count = s->options.size();
            std::string opt_key;
            Instruction opt_value = emitLiteral(nullptr);
            if (!s->options.empty()) {
                const auto& opt = s->options.front();
                if (opt.key != parser::v3::StringPool::INVALID_ID) {
                    opt_key = std::string(pool_.get(opt.key));
                }
                if (opt.value != parser::v3::StringPool::INVALID_ID) {
                    opt_value = makeStringLiteralInstr(std::string(pool_.get(opt.value)));
                }
            }
            payload["options"] = makeOptionKvPlaceholder(opt_count, opt_key, opt_value);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterTablespaceStmt: {
            auto* s = static_cast<parser::v3::AlterTablespaceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_TABLESPACE);
            inst.flags = 0;
            Value::Object payload;
            parser::v3::SchemaPath path;
            if (s->tablespace_name != parser::v3::StringPool::INVALID_ID) {
                path.components.push_back(s->tablespace_name);
            }
            payload["tablespace"] = toSchemaPath(path);
            payload["options"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AttachTablespaceStmt: {
            auto* s = static_cast<parser::v3::AttachTablespaceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ATTACH_TABLESPACE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->tablespace_name);
            payload["location"] = Value(std::string(s->location));
            payload["validate"] = Value(s->validate);
            payload["allow_mismatch"] = Value(s->allow_mismatch);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::DetachTablespaceStmt: {
            auto* s = static_cast<parser::v3::DetachTablespaceStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_DETACH_TABLESPACE);
            inst.flags = 0;
            Value::Object payload;
            payload["name"] = toIdent(s->tablespace_name);
            payload["force"] = Value(s->force);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterDomainStmt: {
            auto* s = static_cast<parser::v3::AlterDomainStmt*>(stmt);
            if (s->action == parser::v3::AlterDomainAction::RENAME) {
                Instruction inst;
                inst.opcode = op(Opcode::SBLR3_RENAME_OBJECT);
                inst.flags = 0;
                Value::Object payload;
                payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(parser::v3::DdlObjectType::DOMAIN)));
                payload["object_path"] = toSchemaPath(s->domain_path);
                payload["new_name"] = toIdent(s->new_name);
                inst.payload = Value(std::move(payload));
                return inst;
            }
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_DOMAIN);
            inst.flags = 0;
            Value::Object payload;
            payload["domain"] = toSchemaPath(s->domain_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            if (!s->value.empty()) payload["value"] = Value(s->value);
            if (s->constraint_name != parser::v3::StringPool::INVALID_ID) payload["constraint"] = toIdent(s->constraint_name);
            if (s->new_name != parser::v3::StringPool::INVALID_ID) payload["new_name"] = toIdent(s->new_name);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterTypeStmt: {
            auto* s = static_cast<parser::v3::AlterTypeStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_DOMAIN);
            inst.flags = 0;
            Value::Object payload;
            payload["type"] = toSchemaPath(s->type_path);
            payload["action"] = Value(uint64_t(static_cast<uint8_t>(s->action)));
            if (s->new_name != parser::v3::StringPool::INVALID_ID) payload["new_name"] = toIdent(s->new_name);
            if (s->new_schema != parser::v3::StringPool::INVALID_ID) payload["new_schema"] = toIdent(s->new_schema);
            if (s->value_label != parser::v3::StringPool::INVALID_ID) payload["value_label"] = toIdent(s->value_label);
            if (s->before_label != parser::v3::StringPool::INVALID_ID) payload["before_label"] = toIdent(s->before_label);
            if (s->after_label != parser::v3::StringPool::INVALID_ID) payload["after_label"] = toIdent(s->after_label);
            if (s->old_label != parser::v3::StringPool::INVALID_ID) payload["old_label"] = toIdent(s->old_label);
            if (s->new_label != parser::v3::StringPool::INVALID_ID) payload["new_label"] = toIdent(s->new_label);
            payload["is_range_options"] = Value(s->is_range_options);
            payload["is_base_options"] = Value(s->is_base_options);
            if (s->is_range_options) {
                Value::Object range;
                if (s->range_options.has_subtype) {
                    range["subtype"] = Value(buildTypeSpec(s->range_options.subtype));
                }
                if (s->range_options.has_subtype_collation) range["subtype_collation"] = Value(s->range_options.subtype_collation);
                if (s->range_options.has_subtype_opclass) range["subtype_opclass"] = Value(s->range_options.subtype_opclass);
                if (s->range_options.has_canonical) range["canonical"] = Value(s->range_options.canonical);
                if (s->range_options.has_subtype_diff) range["subtype_diff"] = Value(s->range_options.subtype_diff);
                if (s->range_options.has_multirange) range["multirange"] = Value(s->range_options.multirange);
                payload["range_options"] = Value(std::move(range));
            }
            if (s->is_base_options) {
                Value::Object base;
                if (s->base_options.has_storage) base["storage"] = Value(buildTypeSpec(s->base_options.storage));
                if (!s->base_options.input_function.empty()) base["input_function"] = Value(s->base_options.input_function);
                if (!s->base_options.output_function.empty()) base["output_function"] = Value(s->base_options.output_function);
                if (s->base_options.has_receive) base["receive_function"] = Value(s->base_options.receive_function);
                if (s->base_options.has_send) base["send_function"] = Value(s->base_options.send_function);
                if (s->base_options.has_typmod_in) base["typmod_in_function"] = Value(s->base_options.typmod_in_function);
                if (s->base_options.has_typmod_out) base["typmod_out_function"] = Value(s->base_options.typmod_out_function);
                if (s->base_options.has_analyze) base["analyze_function"] = Value(s->base_options.analyze_function);
                if (s->base_options.has_alignment) base["alignment"] = Value(uint64_t(static_cast<uint8_t>(s->base_options.alignment)));
                if (s->base_options.has_storage_mode) base["storage_mode"] = Value(uint64_t(static_cast<uint8_t>(s->base_options.storage_mode)));
                if (s->base_options.has_category) base["category"] = Value(uint64_t(static_cast<uint8_t>(s->base_options.category)));
                if (s->base_options.has_preferred) base["preferred"] = Value(s->base_options.preferred);
                payload["base_options"] = Value(std::move(base));
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterPolicyStmt: {
            auto* s = static_cast<parser::v3::AlterPolicyStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_POLICY);
            inst.flags = 0;
            Value::Object payload;
            payload["policy_name"] = toIdent(s->policy_name);
            payload["table"] = toSchemaPath(s->table_path);
            Value::List roles;
            for (auto id : s->roles) roles.push_back(toIdent(id));
            payload["roles"] = Value(std::move(roles));
            if (s->using_expr) payload["using_expr"] = Value(makeInstr(emitExpression(s->using_expr)));
            if (s->with_check_expr) payload["check_expr"] = Value(makeInstr(emitExpression(s->with_check_expr)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterSystemStmt: {
            auto* s = static_cast<parser::v3::AlterSystemStmt*>(stmt);
            auto key_text = std::string(pool_.get(s->name));

            auto emit_multi_model = [&](Opcode opcode, uint8_t action_code) {
                Instruction multi_model_inst;
                multi_model_inst.opcode = op(opcode);
                multi_model_inst.flags = 0;
                Value::Object multi_model_payload;
                multi_model_payload["action"] = Value(uint64_t(action_code));
                if (s->value) {
                    multi_model_payload["query_expr"] =
                        Value(makeInstr(emitExpression(s->value)));
                }
                // OPTION_KV schema payload: empty object encodes deterministic zero-item list.
                multi_model_payload["options"] = Value(Value::Object{});
                multi_model_inst.payload = Value(std::move(multi_model_payload));
                return multi_model_inst;
            };

            if (key_text == "nosql.cql.keyspace") {
                return emit_multi_model(Opcode::SBLR3_CQL_KEYSPACE, 1);
            }
            if (key_text == "nosql.cql.batch") {
                return emit_multi_model(Opcode::SBLR3_CQL_BATCH, 2);
            }
            if (key_text == "nosql.cql.ttl") {
                return emit_multi_model(Opcode::SBLR3_CQL_TTL, 3);
            }
            if (key_text == "nosql.cql.writetime") {
                return emit_multi_model(Opcode::SBLR3_CQL_WRITETIME, 4);
            }
            if (key_text == "nosql.mongo.find") {
                return emit_multi_model(Opcode::SBLR3_MONGO_FIND, 5);
            }
            if (key_text == "nosql.mongo.aggregate") {
                return emit_multi_model(Opcode::SBLR3_MONGO_AGGREGATE, 6);
            }
            if (key_text == "nosql.mongo.find_and_modify") {
                return emit_multi_model(Opcode::SBLR3_MONGO_FIND_AND_MODIFY, 7);
            }
            if (key_text == "nosql.mongo.bulk_write") {
                return emit_multi_model(Opcode::SBLR3_MONGO_BULK_WRITE, 8);
            }
            if (key_text == "nosql.cypher.match") {
                return emit_multi_model(Opcode::SBLR3_CYPHER_MATCH, 9);
            }
            if (key_text == "nosql.cypher.merge") {
                return emit_multi_model(Opcode::SBLR3_CYPHER_MERGE, 10);
            }
            if (key_text == "nosql.cypher.unwind") {
                return emit_multi_model(Opcode::SBLR3_CYPHER_UNWIND, 11);
            }
            if (key_text == "nosql.cypher.call") {
                return emit_multi_model(Opcode::SBLR3_CYPHER_CALL, 12);
            }
            if (key_text == "nosql.redis.string") {
                return emit_multi_model(Opcode::SBLR3_REDIS_STRING, 13);
            }
            if (key_text == "nosql.redis.hash") {
                return emit_multi_model(Opcode::SBLR3_REDIS_HASH, 14);
            }
            if (key_text == "nosql.redis.list") {
                return emit_multi_model(Opcode::SBLR3_REDIS_LIST, 15);
            }
            if (key_text == "nosql.redis.set") {
                return emit_multi_model(Opcode::SBLR3_REDIS_SET, 16);
            }
            if (key_text == "nosql.redis.zset") {
                return emit_multi_model(Opcode::SBLR3_REDIS_ZSET, 17);
            }
            if (key_text == "nosql.redis.stream") {
                return emit_multi_model(Opcode::SBLR3_REDIS_STREAM, 18);
            }
            if (key_text == "nosql.redis.pubsub") {
                return emit_multi_model(Opcode::SBLR3_REDIS_PUBSUB, 19);
            }
            if (key_text == "nosql.milvus.create_collection") {
                return emit_multi_model(Opcode::SBLR3_MILVUS_CREATE_COLLECTION, 20);
            }
            if (key_text == "nosql.milvus.drop_collection") {
                return emit_multi_model(Opcode::SBLR3_MILVUS_DROP_COLLECTION, 21);
            }
            if (key_text == "nosql.milvus.create_index") {
                return emit_multi_model(Opcode::SBLR3_MILVUS_CREATE_INDEX, 22);
            }
            if (key_text == "nosql.milvus.drop_index") {
                return emit_multi_model(Opcode::SBLR3_MILVUS_DROP_INDEX, 23);
            }
            if (key_text == "nosql.milvus.insert") {
                return emit_multi_model(Opcode::SBLR3_MILVUS_INSERT, 24);
            }
            if (key_text == "nosql.milvus.delete") {
                return emit_multi_model(Opcode::SBLR3_MILVUS_DELETE, 25);
            }
            if (key_text == "nosql.milvus.search") {
                return emit_multi_model(Opcode::SBLR3_MILVUS_SEARCH, 26);
            }
            if (key_text == "nosql.milvus.query") {
                return emit_multi_model(Opcode::SBLR3_MILVUS_QUERY, 27);
            }
            if (key_text == "admin.backup") {
                return emit_multi_model(Opcode::SBLR3_ADMIN_BACKUP, 28);
            }
            if (key_text == "admin.restore") {
                return emit_multi_model(Opcode::SBLR3_ADMIN_RESTORE, 29);
            }
            if (key_text == "admin.validate") {
                return emit_multi_model(Opcode::SBLR3_ADMIN_VALIDATE, 30);
            }
            if (key_text.rfind("cluster.workload_class.create.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_WORKLOAD_CLASS, 32);
            }
            if (key_text.rfind("cluster.workload_class.alter.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_WORKLOAD_CLASS, 33);
            }
            if (key_text.rfind("cluster.workload_class.drop.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_WORKLOAD_CLASS, 34);
            }
            if (key_text.rfind("cluster.workload_route.create.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_WORKLOAD_ROUTE, 35);
            }
            if (key_text.rfind("cluster.workload_route.alter.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_WORKLOAD_ROUTE, 36);
            }
            if (key_text.rfind("cluster.workload_route.drop.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_WORKLOAD_ROUTE, 37);
            }
            if (key_text.rfind("cluster.admission_policy.create.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_ADMISSION_POLICY, 38);
            }
            if (key_text.rfind("cluster.admission_policy.alter.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_ADMISSION_POLICY, 39);
            }
            if (key_text.rfind("cluster.admission_policy.drop.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_ADMISSION_POLICY, 40);
            }
            if (key_text.rfind("cluster.admission_binding.create.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_ADMISSION_BINDING, 41);
            }
            if (key_text.rfind("cluster.admission_binding.alter.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_ADMISSION_BINDING, 42);
            }
            if (key_text.rfind("cluster.admission_binding.drop.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_ADMISSION_BINDING, 43);
            }
            if (key_text == "cluster.set_state") {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_SET_STATE, 44);
            }
            if (key_text == "cluster.show_state") {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_SHOW_STATE, 45);
            }
            if (key_text == "cluster.show_routing_plan") {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_SHOW_ROUTING_PLAN, 46);
            }
            if (key_text == "cluster.show_admission_status") {
                return emit_multi_model(Opcode::SBLR3_CLUSTER_SHOW_ADMISSION_STATUS, 47);
            }
            if (key_text == "service.channel.backup") {
                return emit_multi_model(Opcode::SBLR3_SERVICE_CHANNEL_BACKUP, 48);
            }
            if (key_text == "service.channel.events") {
                return emit_multi_model(Opcode::SBLR3_SERVICE_CHANNEL_EVENTS, 49);
            }
            if (key_text == "service.channel.progress") {
                return emit_multi_model(Opcode::SBLR3_SERVICE_CHANNEL_PROGRESS, 50);
            }
            if (key_text.rfind("cube.ddl.create.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CUBE_DDL, 51);
            }
            if (key_text.rfind("cube.ddl.alter.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CUBE_DDL, 52);
            }
            if (key_text.rfind("cube.ddl.drop.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CUBE_DDL, 53);
            }
            if (key_text.rfind("cube.refresh.", 0) == 0) {
                return emit_multi_model(Opcode::SBLR3_CUBE_REFRESH, 54);
            }
            if (key_text == "cube.show_stats") {
                return emit_multi_model(Opcode::SBLR3_CUBE_SHOW_STATS, 55);
            }

            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_SYSTEM);
            inst.flags = 0;
            Value::Object payload;
            payload["key"] = toIdent(s->name);
            if (s->value) payload["value"] = Value(makeInstr(emitExpression(s->value)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterJobStmt: {
            auto* s = static_cast<parser::v3::AlterJobStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_JOB);
            inst.flags = 0;
            Value::Object payload;
            payload["job_name"] = toIdent(s->job_name);
            if (s->has_schedule) {
                payload["schedule_kind"] = Value(uint64_t(static_cast<uint8_t>(s->schedule_kind)));
                if (s->schedule_kind == parser::v3::JobScheduleKind::CRON &&
                    s->cron_expression != parser::v3::StringPool::INVALID_ID) {
                    payload["cron_expression"] = toIdent(s->cron_expression);
                }
                if (s->schedule_kind == parser::v3::JobScheduleKind::AT &&
                    s->at_timestamp != parser::v3::StringPool::INVALID_ID) {
                    payload["at_timestamp"] = toIdent(s->at_timestamp);
                }
                if (s->schedule_kind == parser::v3::JobScheduleKind::EVERY) {
                    payload["interval_seconds"] = Value(int64_t(s->interval_seconds));
                }
                if (s->starts_at != parser::v3::StringPool::INVALID_ID) payload["starts_at"] = toIdent(s->starts_at);
                if (s->ends_at != parser::v3::StringPool::INVALID_ID) payload["ends_at"] = toIdent(s->ends_at);
            }
            if (s->has_job_body) {
                payload["job_type"] = Value(uint64_t(static_cast<uint8_t>(s->job_type)));
                if (s->job_type == parser::v3::JobType::SQL && s->job_sql != parser::v3::StringPool::INVALID_ID) {
                    payload["job_sql"] = Value(std::string(pool_.get(s->job_sql)));
                }
                if (s->job_type == parser::v3::JobType::PROCEDURE &&
                    s->procedure_name != parser::v3::StringPool::INVALID_ID) {
                    payload["procedure_name"] = toIdent(s->procedure_name);
                }
                if (s->job_type == parser::v3::JobType::EXTERNAL &&
                    s->external_command != parser::v3::StringPool::INVALID_ID) {
                    payload["external_command"] = Value(std::string(pool_.get(s->external_command)));
                }
            }
            if (s->has_state) payload["state"] = Value(uint64_t(static_cast<uint8_t>(s->state)));
            if (s->has_max_retries) payload["max_retries"] = Value(uint64_t(s->max_retries));
            if (s->has_retry_backoff) payload["retry_backoff_seconds"] = Value(uint64_t(s->retry_backoff_seconds));
            if (s->has_timeout) payload["timeout_seconds"] = Value(uint64_t(s->timeout_seconds));
            if (s->has_on_completion) payload["on_completion"] = Value(uint64_t(static_cast<uint8_t>(s->on_completion)));
            if (s->has_run_as && s->run_as_role != parser::v3::StringPool::INVALID_ID) payload["run_as_role"] = toIdent(s->run_as_role);
            if (s->has_description && s->description != parser::v3::StringPool::INVALID_ID) {
                payload["description"] = Value(std::string(pool_.get(s->description)));
            }
            if (s->has_job_class && s->job_class != parser::v3::StringPool::INVALID_ID) payload["job_class"] = toIdent(s->job_class);
            if (s->has_partition) {
                if (s->partition_strategy != parser::v3::StringPool::INVALID_ID) payload["partition_strategy"] = toIdent(s->partition_strategy);
                if (s->partition_expression != parser::v3::StringPool::INVALID_ID) payload["partition_expression"] = toIdent(s->partition_expression);
                if (s->partition_shard != parser::v3::StringPool::INVALID_ID) payload["partition_shard"] = toIdent(s->partition_shard);
            }
            Value::List depends;
            for (auto id : s->depends_on) depends.push_back(toIdent(id));
            payload["depends_on"] = Value(std::move(depends));
            payload["clear_depends_on"] = Value(s->clear_depends_on);
            if (s->has_secret) {
                if (s->secret_key != parser::v3::StringPool::INVALID_ID) payload["secret_key"] = toIdent(s->secret_key);
                if (s->secret_value != parser::v3::StringPool::INVALID_ID) {
                    payload["secret_value"] = Value(std::string(pool_.get(s->secret_value)));
                }
            }
            payload["drop_secret"] = Value(s->drop_secret);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::RenameObjectStmt: {
            auto* s = static_cast<parser::v3::RenameObjectStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_RENAME_OBJECT);
            inst.flags = 0;
            Value::Object payload;
            payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(s->object_type)));
            payload["object_path"] = toSchemaPath(s->object_path);
            payload["new_name"] = toIdent(s->new_name);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::MoveObjectStmt: {
            auto* s = static_cast<parser::v3::MoveObjectStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_MOVE_OBJECT);
            inst.flags = 0;
            Value::Object payload;
            payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(s->object_type)));
            payload["object_path"] = toSchemaPath(s->object_path);
            std::string target_name;
            if (s->has_new_name && s->new_name != parser::v3::StringPool::INVALID_ID) {
                target_name = std::string(pool_.get(s->new_name));
            } else if (!s->object_path.components.empty()) {
                target_name = std::string(pool_.get(s->object_path.components.back()));
            }

            std::string schema_prefix;
            for (size_t i = 0; i < s->target_schema.components.size(); ++i) {
                if (i > 0) {
                    schema_prefix.push_back('.');
                }
                schema_prefix += std::string(pool_.get(s->target_schema.components[i]));
            }

            std::string full_name;
            if (!schema_prefix.empty()) {
                full_name = schema_prefix;
                if (!target_name.empty()) {
                    full_name.push_back('.');
                    full_name += target_name;
                }
            } else {
                full_name = target_name;
            }
            payload["new_name"] = Value(std::move(full_name));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.flags = 0;
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDdlDrop(parser::v3::Statement* stmt) {
    auto makeDrop = [&](Opcode opcode, const parser::v3::SchemaPath& path, uint8_t object_type) {
        Instruction inst;
        inst.opcode = op(opcode);
        inst.flags = 0;
        Value::Object payload;
        payload["flags"] = Value(uint64_t(0));
        payload["object_type"] = Value(uint64_t(object_type));
        payload["path"] = toSchemaPath(path);
        inst.payload = Value(std::move(payload));
        return inst;
    };

    switch (stmt->kind()) {
        case parser::v3::ASTKind::DropTableStmt:
            return makeDrop(Opcode::SBLR3_DROP_TABLE, static_cast<parser::v3::DropTableStmt*>(stmt)->tables.front(), 1);
        case parser::v3::ASTKind::DropIndexStmt:
            return makeDrop(Opcode::SBLR3_DROP_INDEX, static_cast<parser::v3::DropIndexStmt*>(stmt)->indexes.front(), 2);
        case parser::v3::ASTKind::DropViewStmt:
            return makeDrop(Opcode::SBLR3_DROP_VIEW, static_cast<parser::v3::DropViewStmt*>(stmt)->views.front(), 3);
        case parser::v3::ASTKind::DropSequenceStmt:
            return makeDrop(Opcode::SBLR3_DROP_SEQUENCE, static_cast<parser::v3::DropSequenceStmt*>(stmt)->sequences.front(), 4);
        case parser::v3::ASTKind::DropSchemaStmt:
            return makeDrop(Opcode::SBLR3_DROP_SCHEMA, static_cast<parser::v3::DropSchemaStmt*>(stmt)->schemas.front(), 5);
        case parser::v3::ASTKind::DropDatabaseStmt:
            return makeDrop(Opcode::SBLR3_DROP_DATABASE, static_cast<parser::v3::DropDatabaseStmt*>(stmt)->database_path, 6);
        case parser::v3::ASTKind::DropTablespaceStmt: {
            auto* s = static_cast<parser::v3::DropTablespaceStmt*>(stmt);
            parser::v3::SchemaPath path(parser::v3::PathType::UNQUALIFIED,
                                        {s->tablespace_name});
            return makeDrop(Opcode::SBLR3_DROP_TABLESPACE, path, 16);
        }
        case parser::v3::ASTKind::DropDomainStmt:
            return makeDrop(Opcode::SBLR3_DROP_DOMAIN, static_cast<parser::v3::DropDomainStmt*>(stmt)->domains.front(), 7);
        case parser::v3::ASTKind::DropTypeStmt:
            return makeDrop(Opcode::SBLR3_DROP_DOMAIN, static_cast<parser::v3::DropTypeStmt*>(stmt)->types.front(), 8);
        case parser::v3::ASTKind::DropFunctionStmt:
            return makeDrop(Opcode::SBLR3_DROP_FUNCTION_STMT, static_cast<parser::v3::DropFunctionStmt*>(stmt)->functions.front(), 9);
        case parser::v3::ASTKind::DropProcedureStmt:
            return makeDrop(Opcode::SBLR3_DROP_PROCEDURE_STMT, static_cast<parser::v3::DropProcedureStmt*>(stmt)->procedures.front(), 10);
        case parser::v3::ASTKind::DropTriggerStmt:
            return makeDrop(Opcode::SBLR3_DROP_TRIGGER, static_cast<parser::v3::DropTriggerStmt*>(stmt)->triggers.front(), 11);
        case parser::v3::ASTKind::DropPackageStmt:
            return makeDrop(Opcode::SBLR3_DROP_PACKAGE_STMT, static_cast<parser::v3::DropPackageStmt*>(stmt)->packages.front(), 12);
        case parser::v3::ASTKind::DropRoleStmt:
            return makeDrop(Opcode::SBLR3_DROP_ROLE, static_cast<parser::v3::DropRoleStmt*>(stmt)->roles.front(), 13);
        case parser::v3::ASTKind::DropGroupStmt:
            return makeDrop(Opcode::SBLR3_DROP_GROUP, static_cast<parser::v3::DropGroupStmt*>(stmt)->groups.front(), 15);
        case parser::v3::ASTKind::DropUserStmt:
            return makeDrop(Opcode::SBLR3_DROP_USER, static_cast<parser::v3::DropUserStmt*>(stmt)->users.front(), 14);
        case parser::v3::ASTKind::DropExceptionStmt:
            return makeDrop(Opcode::SBLR3_DROP_EXCEPTION_STMT, static_cast<parser::v3::DropExceptionStmt*>(stmt)->exceptions.front(), 24);
        case parser::v3::ASTKind::DropPolicyStmt: {
            auto* s = static_cast<parser::v3::DropPolicyStmt*>(stmt);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_DROP_POLICY);
            inst.flags = 0;
            Value::Object payload;
            payload["policy_name"] = toIdent(s->policy_name);
            payload["table"] = toSchemaPath(s->table_path);
            payload["flags"] = Value(uint64_t(s->if_exists ? 0x01 : 0x00));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::DropForeignServerStmt:
            {
                auto* s = static_cast<parser::v3::DropForeignServerStmt*>(stmt);
                parser::v3::SchemaPath path;
                path.components.push_back(s->server_name);
                return makeDrop(Opcode::SBLR3_DROP_FOREIGN_SERVER, path, 31);
            }
        case parser::v3::ASTKind::DropForeignTableStmt:
            return makeDrop(Opcode::SBLR3_DROP_FOREIGN_TABLE, static_cast<parser::v3::DropForeignTableStmt*>(stmt)->tables.front(), 32);
        case parser::v3::ASTKind::DropUserMappingStmt:
            {
                auto* s = static_cast<parser::v3::DropUserMappingStmt*>(stmt);
                parser::v3::SchemaPath path;
                if (s->server_name != parser::v3::StringPool::INVALID_ID) {
                    path.components.push_back(s->server_name);
                }
                if (s->user_name != parser::v3::StringPool::INVALID_ID) {
                    path.components.push_back(s->user_name);
                }
                return makeDrop(Opcode::SBLR3_DROP_USER_MAPPING, path, 33);
            }
        case parser::v3::ASTKind::DropSynonymStmt:
            return makeDrop(Opcode::SBLR3_DROP_SYNONYM, static_cast<parser::v3::DropSynonymStmt*>(stmt)->synonyms.front(), 38);
        case parser::v3::ASTKind::DropUdrStmt:
            return makeDrop(Opcode::SBLR3_DROP_UDR, static_cast<parser::v3::DropUdrStmt*>(stmt)->udrs.front(), 23);
        case parser::v3::ASTKind::DropJobStmt:
            {
                auto* s = static_cast<parser::v3::DropJobStmt*>(stmt);
                parser::v3::SchemaPath path;
                path.components.push_back(s->job_name);
                return makeDrop(Opcode::SBLR3_DROP_JOB, path, 0);
            }
        default:
            break;
    }
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.flags = 0;
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitDdlTruncate(parser::v3::TruncateTableStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_TRUNCATE_TABLE);
    inst.flags = 0;
    Value::Object payload;
    payload["flags"] = Value(uint64_t(0));
    Value::List tables;
    for (const auto& path : stmt->tables) tables.push_back(toSchemaPath(path));
    payload["tables"] = Value(std::move(tables));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitComment(parser::v3::CommentStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_COMMENT);
    inst.flags = 0;
    Value::Object payload;
    payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(stmt->object_type)));
    payload["object_path"] = toSchemaPath(stmt->object_path);
    payload["action"] = Value(uint64_t(static_cast<uint8_t>(stmt->action)));
    payload["is_null"] = Value(stmt->is_null || stmt->action != parser::v3::CommentStmt::Action::SET);
    payload["text"] = Value(stmt->is_null ? std::string() : std::string(pool_.get(stmt->comment_text)));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitGrant(parser::v3::GrantStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_GRANT);
    inst.flags = 0;
    Value::Object payload;
    payload["is_grant"] = Value(true);
    uint64_t privs = 0;
    for (auto p : stmt->privileges) {
        privs |= privilegeToCatalogMask(p);
    }
    payload["privileges"] = Value(privs);
    payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(stmt->object_type)));
    if (!stmt->objects.empty()) {
        payload["object_path"] = toSchemaPath(stmt->objects.front());
    }
    Value::List grantees;
    if (stmt->is_public) {
        grantees.push_back(Value(std::string("PUBLIC")));
    } else {
        for (auto id : stmt->grantees) grantees.push_back(toIdent(id));
    }
    payload["grantees"] = Value(std::move(grantees));
    payload["with_grant_option"] = Value(stmt->with_grant_option);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitRevoke(parser::v3::RevokeStmt* stmt) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_REVOKE);
    inst.flags = 0;
    Value::Object payload;
    payload["is_grant"] = Value(false);
    uint64_t privs = 0;
    for (auto p : stmt->privileges) {
        privs |= privilegeToCatalogMask(p);
    }
    payload["privileges"] = Value(privs);
    payload["object_type"] = Value(uint64_t(static_cast<uint8_t>(stmt->object_type)));
    if (!stmt->objects.empty()) {
        payload["object_path"] = toSchemaPath(stmt->objects.front());
    }
    Value::List grantees;
    if (stmt->is_public) {
        grantees.push_back(Value(std::string("PUBLIC")));
    } else {
        for (auto id : stmt->grantees) grantees.push_back(toIdent(id));
    }
    payload["grantees"] = Value(std::move(grantees));
    payload["with_grant_option"] = Value(stmt->grant_option_for);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitTxn(parser::v3::Statement* stmt) {
    Instruction inst;
    inst.flags = 0;
    Value::Object payload;

    switch (stmt->kind()) {
        case parser::v3::ASTKind::StartTransactionStmt:
        {
            inst.opcode = op(Opcode::SBLR3_START_TRANSACTION);
            payload["action"] = Value(uint64_t(1));
            auto* s = static_cast<parser::v3::StartTransactionStmt*>(stmt);
            if (s->has_isolation_level)
            {
                payload["isolation"] = Value(uint64_t(static_cast<uint8_t>(s->isolation_level)));
            }
            if (s->has_read_committed_mode)
            {
                payload["read_committed_mode"] =
                    Value(uint64_t(static_cast<uint8_t>(s->read_committed_mode)));
            }
            if (s->has_access_mode)
            {
                payload["access_mode"] = Value(uint64_t(static_cast<uint8_t>(s->access_mode)));
            }
            if (s->has_wait_mode)
            {
                payload["wait_mode"] = Value(uint64_t(static_cast<uint8_t>(s->wait_mode)));
            }
            if (s->has_lock_timeout)
            {
                payload["lock_timeout"] = Value(uint64_t(s->lock_timeout_seconds));
            }
            if (s->deferrable)
            {
                payload["deferrable"] = Value(true);
            }
            else if (s->not_deferrable)
            {
                payload["deferrable"] = Value(false);
            }
            if (s->has_autocommit)
            {
                payload["autocommit_mode"] =
                    Value(uint64_t(static_cast<uint8_t>(s->autocommit_mode)));
            }
            if (s->conflict_action != parser::v3::TransactionConflictAction::DEFAULT)
            {
                payload["conflict_action"] =
                    Value(uint64_t(static_cast<uint8_t>(s->conflict_action)));
            }
            if (s->has_conflict_error_code)
            {
                payload["conflict_error_code"] = Value(int64_t(s->conflict_error_code));
            }
            if (!s->table_reservations.empty())
            {
                Value::List reservations;
                reservations.reserve(s->table_reservations.size());
                for (const auto& r : s->table_reservations)
                {
                    Value::Object entry;
                    entry["table_name"] = toIdent(r.table_name);
                    entry["lock_mode"] = Value(uint64_t(static_cast<uint8_t>(r.lock_mode)));
                    entry["for_write"] = Value(r.for_write);
                    reservations.emplace_back(Value(std::move(entry)));
                }
                payload["reservations"] = Value(std::move(reservations));
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::PrepareTransactionStmt:
            inst.opcode = op(Opcode::SBLR3_PREPARE_TRANSACTION);
            payload["action"] = Value(uint64_t(0));
            payload["name"] = toIdent(static_cast<parser::v3::PrepareTransactionStmt*>(stmt)->gid);
            inst.payload = Value(std::move(payload));
            return inst;
        case parser::v3::ASTKind::CommitStmt: {
            auto* s = static_cast<parser::v3::CommitStmt*>(stmt);
            if (s->is_prepared) {
                inst.opcode = op(Opcode::SBLR3_COMMIT_PREPARED);
                payload["name"] = toIdent(s->prepared_gid);
            } else if (s->retaining) {
                inst.opcode = op(Opcode::SBLR3_COMMIT_RETAINING);
            } else {
                inst.opcode = op(Opcode::SBLR3_COMMIT);
            }
            payload["action"] = Value(uint64_t(2));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::RollbackStmt: {
            auto* s = static_cast<parser::v3::RollbackStmt*>(stmt);
            if (s->is_prepared) {
                inst.opcode = op(Opcode::SBLR3_ROLLBACK_PREPARED);
                payload["name"] = toIdent(s->prepared_gid);
            } else if (s->retaining) {
                inst.opcode = op(Opcode::SBLR3_ROLLBACK_RETAINING);
            } else if (s->to_savepoint) {
                inst.opcode = op(Opcode::SBLR3_ROLLBACK_TO_SAVEPOINT);
                payload["action"] = Value(uint64_t(6));
                payload["name"] = toIdent(s->savepoint_name);
                inst.payload = Value(std::move(payload));
                return inst;
            } else {
                inst.opcode = op(Opcode::SBLR3_ROLLBACK);
            }
            payload["action"] = Value(uint64_t(3));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::SavepointStmt:
            inst.opcode = op(Opcode::SBLR3_SAVEPOINT);
            payload["action"] = Value(uint64_t(4));
            payload["name"] = toIdent(static_cast<parser::v3::SavepointStmt*>(stmt)->name);
            inst.payload = Value(std::move(payload));
            return inst;
        case parser::v3::ASTKind::ReleaseSavepointStmt:
            inst.opcode = op(Opcode::SBLR3_RELEASE_SAVEPOINT);
            payload["action"] = Value(uint64_t(5));
            payload["name"] = toIdent(static_cast<parser::v3::ReleaseSavepointStmt*>(stmt)->name);
            inst.payload = Value(std::move(payload));
            return inst;
        default:
            break;
    }

    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitSetShowReset(parser::v3::Statement* stmt) {
    Instruction inst;
    inst.flags = 0;
    Value::Object payload;
    auto makeStringLiteral = [&](const std::string& text) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(text)}});
        return lit;
    };
    auto makeIntLiteral = [&](int64_t value) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_INT64);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(value)}});
        return lit;
    };
    auto makeBoolLiteral = [&](bool value) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_BOOLEAN);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(value)}});
        return lit;
    };
    auto setValueInstr = [&](Instruction value) {
        payload["value"] = Value(makeInstr(std::move(value)));
    };
    auto setValueStringId = [&](parser::v3::StringPool::StringId id) {
        if (id != parser::v3::StringPool::INVALID_ID) {
            setValueInstr(makeStringLiteral(std::string(pool_.get(id))));
        }
    };
    auto normalizeKey = [&](parser::v3::StringPool::StringId id) -> Value {
        if (id == parser::v3::StringPool::INVALID_ID) return Value(std::string());
        std::string key(pool_.get(id));
        if (key == "TIME_ZONE") key = "TIME ZONE";
        if (key == "SESSION_AUTHORIZATION") key = "SESSION AUTHORIZATION";
        return Value(std::move(key));
    };

    if (stmt->kind() == parser::v3::ASTKind::AnalyzeStmt) {
        inst.opcode = op(Opcode::SBLR3_ANALYZE);
        auto* s = static_cast<parser::v3::AnalyzeStmt*>(stmt);
        payload["target"] = Value(uint64_t(
            s->target == parser::v3::AnalyzeStmt::AnalyzeTarget::INDEX ? 2 : 1));
        if (s->target == parser::v3::AnalyzeStmt::AnalyzeTarget::INDEX) {
            payload["index_path"] = toSchemaPath(s->index_path);
        } else {
            payload["table_path"] = toSchemaPath(s->table_path);
        }
        if (s->has_column) payload["column"] = toIdent(s->column_name);
        if (s->has_sample) payload["sample_rate"] = Value(s->sample_rate);
        payload["verbose"] = Value(s->verbose);
        inst.payload = Value(std::move(payload));
        return inst;
    }

    if (stmt->kind() == parser::v3::ASTKind::ExplainStmt) {
        inst.opcode = op(Opcode::SBLR3_EXPLAIN_PLAN);
        auto* s = static_cast<parser::v3::ExplainStmt*>(stmt);
        payload["analyze"] = Value(s->analyze);
        payload["verbose"] = Value(s->verbose);
        payload["costs"] = Value(s->costs);
        payload["buffers"] = Value(s->buffers);
        payload["wal"] = Value(s->wal);
        payload["timing"] = Value(s->timing);
        if (s->format_json) {
            payload["format"] = Value(std::string("JSON"));
        } else if (s->format_xml) {
            payload["format"] = Value(std::string("XML"));
        } else if (s->format_yaml) {
            payload["format"] = Value(std::string("YAML"));
        }
        if (s->query) payload["query"] = Value(makeInstr(emitStatement(s->query)));
        inst.payload = Value(std::move(payload));
        return inst;
    }

    if (stmt->kind() == parser::v3::ASTKind::SetStmt) {
        auto* s = static_cast<parser::v3::SetStmt*>(stmt);
        payload["action"] = Value(uint64_t(1));
        payload["scope"] = Value(uint64_t(s->scope == parser::v3::SetStmt::Scope::LOCAL ? 1 : 0));

        switch (s->set_type) {
            case parser::v3::SetStmt::SetType::TIME_ZONE:
                inst.opcode = op(Opcode::SBLR3_SET_VARIABLE);
                payload["key"] = Value(std::string("TIME ZONE"));
                if (!s->is_default && s->value) setValueInstr(emitExpression(s->value));
                break;
            case parser::v3::SetStmt::SetType::AUTOCOMMIT:
                inst.opcode = op(Opcode::SBLR3_SET_AUTOCOMMIT);
                payload["key"] = Value(std::string("AUTOCOMMIT"));
                if (s->has_autocommit) {
                    bool on = (s->autocommit_mode == parser::v3::AutocommitMode::ON);
                    setValueInstr(makeBoolLiteral(on));
                }
                break;
            case parser::v3::SetStmt::SetType::TRANSACTION:
                inst.opcode = op(Opcode::SBLR3_SET_TRANSACTION);
                payload["key"] = Value(std::string("TRANSACTION"));
                break;
            case parser::v3::SetStmt::SetType::CONSTRAINTS:
            {
                inst.opcode = op(Opcode::SBLR3_SET_VARIABLE);
                std::string key = "CONSTRAINTS";
                if (s->constraints_all) {
                    key += " ALL";
                } else if (!s->constraint_names.empty()) {
                    key += " ";
                    for (size_t i = 0; i < s->constraint_names.size(); ++i) {
                        if (i > 0) key += ",";
                        key += std::string(pool_.get(s->constraint_names[i]));
                    }
                }
                payload["key"] = Value(key);
                setValueInstr(makeBoolLiteral(s->constraints_deferred));
                break;
            }
            case parser::v3::SetStmt::SetType::SQL_DIALECT:
                inst.opcode = op(Opcode::SBLR3_SET_SQL_DIALECT);
                payload["key"] = Value(std::string("SQL DIALECT"));
                if (s->sql_dialect != 0) setValueInstr(makeIntLiteral(s->sql_dialect));
                break;
            case parser::v3::SetStmt::SetType::NAMES:
                inst.opcode = op(Opcode::SBLR3_SET_NAMES);
                payload["key"] = Value(std::string("NAMES"));
                setValueStringId(s->name);
                break;
            case parser::v3::SetStmt::SetType::LOCAL_TIMEOUT:
                inst.opcode = op(Opcode::SBLR3_SET_LOCAL_TIMEOUT);
                payload["key"] = Value(std::string("LOCAL_TIMEOUT"));
                setValueInstr(makeIntLiteral(static_cast<int64_t>(s->local_timeout_seconds)));
                break;
            case parser::v3::SetStmt::SetType::SESSION_AUTHORIZATION:
                inst.opcode = op(Opcode::SBLR3_SET_SESSION_AUTH);
                payload["key"] = Value(std::string("SESSION AUTHORIZATION"));
                if (!s->is_default) setValueStringId(s->name);
                break;
            case parser::v3::SetStmt::SetType::ROLE:
                inst.opcode = op(Opcode::SBLR3_SET_ROLE);
                payload["key"] = Value(std::string("ROLE"));
                if (!s->is_default) setValueStringId(s->name);
                break;
            case parser::v3::SetStmt::SetType::VARIABLE:
            case parser::v3::SetStmt::SetType::TERM:
            case parser::v3::SetStmt::SetType::STATISTICS_INDEX:
            case parser::v3::SetStmt::SetType::GENERATOR:
            default:
                inst.opcode = op(Opcode::SBLR3_SET_VARIABLE);
                payload["key"] = normalizeKey(s->name);
                if (!s->is_default && s->value) {
                    setValueInstr(emitExpression(s->value));
                }
                break;
        }

        inst.payload = Value(std::move(payload));
        return inst;
    }

    if (stmt->kind() == parser::v3::ASTKind::ResetStmt) {
        auto* s = static_cast<parser::v3::ResetStmt*>(stmt);
        payload["action"] = Value(uint64_t(3));
        payload["scope"] = Value(uint64_t(0));
        if (s->reset_all) {
            inst.opcode = op(Opcode::SBLR3_SET_VARIABLE);
            payload["key"] = Value(std::string("ALL"));
        } else {
            std::string key;
            if (s->name != parser::v3::StringPool::INVALID_ID) {
                key = std::string(pool_.get(s->name));
            }
            std::string upper = toUpper(key);

            if (upper == "ROLE") {
                inst.opcode = op(Opcode::SBLR3_SET_ROLE);
                payload["key"] = Value(std::string("ROLE"));
            } else if (upper == "SESSION_AUTHORIZATION" ||
                       upper == "SESSION AUTHORIZATION") {
                inst.opcode = op(Opcode::SBLR3_SET_SESSION_AUTH);
                payload["key"] = Value(std::string("SESSION AUTHORIZATION"));
            } else {
                inst.opcode = op(Opcode::SBLR3_SET_VARIABLE);
                payload["key"] = normalizeKey(s->name);
            }
        }
        inst.payload = Value(std::move(payload));
        return inst;
    }

    if (stmt->kind() == parser::v3::ASTKind::ShowStmt) {
        auto* s = static_cast<parser::v3::ShowStmt*>(stmt);
        payload["action"] = Value(uint64_t(2));
        payload["scope"] = Value(uint64_t(0));

        auto setKeyFromName = [&]() { payload["key"] = normalizeKey(s->name); };
        auto setKeyFromFrom = [&]() { payload["key"] = normalizeKey(s->from_name); };
        auto setValueFromLike = [&]() { setValueStringId(s->like_pattern); };
        auto setUnifiedMetadataPayload = [&]() {
            if (s->metadata_object_type != parser::v3::StringPool::INVALID_ID) {
                payload["metadata_object_type"] = toIdent(s->metadata_object_type);
            }
            if (s->from_name != parser::v3::StringPool::INVALID_ID) {
                payload["path"] = toIdent(s->from_name);
            }
            if (s->name != parser::v3::StringPool::INVALID_ID) {
                payload["name"] = toIdent(s->name);
            }
            if (s->like_pattern != parser::v3::StringPool::INVALID_ID) {
                payload["like"] = toIdent(s->like_pattern);
            }
            if (s->recursive) {
                payload["recursive"] = Value(true);
            }
            if (s->max_depth > 0) {
                payload["max_depth"] = Value(uint64_t(s->max_depth));
            }
            if (s->unified_metadata) {
                payload["metadata_mode"] = Value(std::string(s->is_describe ? "DESCRIBE" : "SHOW"));
            }
            if (s->is_describe) {
                payload["describe_mode"] =
                    Value(uint64_t(static_cast<uint8_t>(s->describe_mode)));
            }
        };

        switch (s->show_type) {
            case parser::v3::ShowStmt::ShowType::ALL:
                inst.opcode = op(Opcode::SBLR3_SHOW_ALL);
                payload["key"] = Value(std::string("ALL"));
                break;
            case parser::v3::ShowStmt::ShowType::TRANSACTION_ISOLATION_LEVEL:
                inst.opcode = op(Opcode::SBLR3_SHOW_TRANSACTION_LEVEL);
                payload["key"] = Value(std::string("TRANSACTION ISOLATION LEVEL"));
                break;
            case parser::v3::ShowStmt::ShowType::TABLES:
                inst.opcode = op(Opcode::SBLR3_SHOW_TABLES);
                setKeyFromFrom();
                setValueFromLike();
                break;
            case parser::v3::ShowStmt::ShowType::DATABASES:
                inst.opcode = op(Opcode::SBLR3_SHOW_DATABASES);
                payload["key"] = Value(std::string());
                setValueFromLike();
                break;
            case parser::v3::ShowStmt::ShowType::COLUMNS:
                inst.opcode = op(Opcode::SBLR3_SHOW_COLUMNS);
                setKeyFromFrom();
                setValueFromLike();
                break;
            case parser::v3::ShowStmt::ShowType::INDEXES:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEXES);
                setKeyFromFrom();
                break;
            case parser::v3::ShowStmt::ShowType::CREATE_TABLE:
                inst.opcode = op(Opcode::SBLR3_SHOW_CREATE_TABLE);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::TABLE:
                inst.opcode = op(Opcode::SBLR3_SHOW_TABLE);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::INDEX:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEX);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::INDEX_HEALTH:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEX);
                setKeyFromName();
                setValueInstr(makeStringLiteral("HEALTH"));
                break;
            case parser::v3::ShowStmt::ShowType::INDEX_USAGE:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEX);
                setKeyFromName();
                setValueInstr(makeStringLiteral("USAGE"));
                break;
            case parser::v3::ShowStmt::ShowType::INDEX_STORAGE:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEX);
                setKeyFromName();
                setValueInstr(makeStringLiteral("STORAGE"));
                break;
            case parser::v3::ShowStmt::ShowType::INDEX_CONTENTION:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEX);
                setKeyFromName();
                setValueInstr(makeStringLiteral("CONTENTION"));
                break;
            case parser::v3::ShowStmt::ShowType::INDEX_OPTIONS:
                inst.opcode = op(Opcode::SBLR3_SHOW_INDEX);
                setKeyFromName();
                setValueInstr(makeStringLiteral("OPTIONS"));
                break;
            case parser::v3::ShowStmt::ShowType::TRIGGER:
                inst.opcode = op(Opcode::SBLR3_SHOW_TRIGGER);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::VIEW:
                inst.opcode = op(Opcode::SBLR3_SHOW_VIEW);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::PROCEDURE:
                inst.opcode = op(Opcode::SBLR3_SHOW_PROCEDURE);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::FUNCTION:
                inst.opcode = op(Opcode::SBLR3_SHOW_FUNCTION);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::DOMAIN:
                inst.opcode = op(Opcode::SBLR3_SHOW_DOMAIN);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::GENERATOR:
                inst.opcode = op(Opcode::SBLR3_SHOW_GENERATOR);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::SCHEMA:
                inst.opcode = op(Opcode::SBLR3_SHOW_SCHEMA);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::ROLE:
                inst.opcode = op(Opcode::SBLR3_SHOW_ROLE);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::GRANTS:
                inst.opcode = op(Opcode::SBLR3_SHOW_GRANTS);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::JOBS:
                inst.opcode = op(Opcode::SBLR3_SHOW_JOBS);
                payload["key"] = Value(std::string());
                setValueFromLike();
                break;
            case parser::v3::ShowStmt::ShowType::JOB:
                inst.opcode = op(Opcode::SBLR3_SHOW_JOB);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::JOB_RUNS:
                inst.opcode = op(Opcode::SBLR3_SHOW_JOB_RUNS);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::CHECKS:
                inst.opcode = op(Opcode::SBLR3_SHOW_CHECKS);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::COLLATIONS:
                inst.opcode = op(Opcode::SBLR3_SHOW_COLLATIONS);
                payload["key"] = Value(std::string());
                setValueFromLike();
                break;
            case parser::v3::ShowStmt::ShowType::COMMENTS:
                inst.opcode = op(Opcode::SBLR3_SHOW_COMMENTS);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::DEPENDENCIES:
                inst.opcode = op(Opcode::SBLR3_SHOW_DEPENDENCIES);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::PACKAGE:
                inst.opcode = op(Opcode::SBLR3_SHOW_PACKAGE);
                setKeyFromName();
                break;
            case parser::v3::ShowStmt::ShowType::SQL_DIALECT:
                inst.opcode = op(Opcode::SBLR3_SHOW_SQL_DIALECT);
                payload["key"] = Value(std::string("SQL DIALECT"));
                break;
            case parser::v3::ShowStmt::ShowType::VERSION:
                inst.opcode = op(Opcode::SBLR3_SHOW_VERSION);
                payload["key"] = Value(std::string());
                break;
            case parser::v3::ShowStmt::ShowType::DATABASE:
                inst.opcode = op(Opcode::SBLR3_SHOW_DATABASE);
                payload["key"] = Value(std::string());
                break;
            case parser::v3::ShowStmt::ShowType::SYSTEM:
                inst.opcode = op(Opcode::SBLR3_SHOW_SYSTEM);
                payload["key"] = Value(std::string());
                break;
            case parser::v3::ShowStmt::ShowType::METRICS:
                inst.opcode = op(Opcode::SBLR3_SHOW_METRICS);
                payload["key"] = Value(std::string());
                break;
            case parser::v3::ShowStmt::ShowType::OBJECTS:
                inst.opcode = op(Opcode::SBLR3_SHOW_OBJECTS);
                payload["key"] = Value(std::string());
                break;
            case parser::v3::ShowStmt::ShowType::VARIABLE:
            default:
                inst.opcode = op(Opcode::SBLR3_SHOW_VARIABLE);
                setKeyFromName();
                break;
        }

        setUnifiedMetadataPayload();
        inst.payload = Value(std::move(payload));
        return inst;
    }

    inst.opcode = op(Opcode::SBLR3_SET_VARIABLE);
    payload["action"] = Value(uint64_t(1));
    payload["key"] = Value(std::string(""));
    payload["scope"] = Value(uint64_t(0));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitUtility(parser::v3::Statement* stmt) {
    Instruction inst;
    inst.flags = 0;

    switch (stmt->kind()) {
        case parser::v3::ASTKind::ConnectStmt:
            inst.opcode = op(Opcode::SBLR3_CONNECT);
            {
                auto* s = static_cast<parser::v3::ConnectStmt*>(stmt);
                Value::Object payload;
                payload["database"] = toIdent(s->database);
                if (s->user != parser::v3::StringPool::INVALID_ID) payload["user"] = toIdent(s->user);
                if (s->password != parser::v3::StringPool::INVALID_ID) payload["password"] = toIdent(s->password);
                if (s->role != parser::v3::StringPool::INVALID_ID) payload["role"] = toIdent(s->role);
                if (s->charset != parser::v3::StringPool::INVALID_ID) payload["charset"] = toIdent(s->charset);
                inst.payload = Value(std::move(payload));
            }
            return inst;
        case parser::v3::ASTKind::DisconnectStmt:
            inst.opcode = op(Opcode::SBLR3_DISCONNECT);
            {
                auto* s = static_cast<parser::v3::DisconnectStmt*>(stmt);
                Value::Object payload;
                payload["target"] = Value(uint64_t(static_cast<uint8_t>(s->target)));
                if (s->connection_name != parser::v3::StringPool::INVALID_ID) {
                    payload["connection_name"] = toIdent(s->connection_name);
                }
                inst.payload = Value(std::move(payload));
            }
            return inst;
        case parser::v3::ASTKind::SweepDatabaseStmt:
            inst.opcode = op(Opcode::SBLR3_SWEEP);
            inst.payload = Value(Value::Object{});
            return inst;
        case parser::v3::ASTKind::ExecuteJobStmt: {
            auto* s = static_cast<parser::v3::ExecuteJobStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_EXECUTE_JOB);
            Value::Object payload;
            payload["job_name"] = toIdent(s->job_name);
            payload["params"] = Value(Value::Object{
                {"count", Value(uint64_t(0))},
                {"key", Value(std::string())},
                {"value", Value(makeInstr(emitLiteral(nullptr)))}});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CancelJobRunStmt: {
            auto* s = static_cast<parser::v3::CancelJobRunStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CANCEL_JOB_RUN);
            uint64_t run_id = 0;
            if (s->job_run_uuid != parser::v3::StringPool::INVALID_ID) {
                std::string v(pool_.get(s->job_run_uuid));
                try {
                    run_id = std::stoull(v);
                } catch (...) {
                    run_id = 0;
                }
            }
            Value::Object payload;
            payload["run_id"] = Value(run_id);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AST_DOC_PATH_FILTER: {
            auto* s = static_cast<parser::v3::DocPathFilterStmt*>(stmt);
            Value::Object fields;
            fields["path_expr"] = Value(s->path_expr);
            fields["operator"] = Value(static_cast<uint64_t>(s->compare_op));
            fields["value_expr"] = Value(s->value_expr);
            std::string err;
            if (!emitVNextContractInstruction(stmt->kind(), fields, inst, err)) {
                fail(err);
            }
            return inst;
        }
        case parser::v3::ASTKind::AST_TS_BUCKET_AGG: {
            auto* s = static_cast<parser::v3::TsBucketAggStmt*>(stmt);
            Value::Object fields;
            fields["time_expr"] = Value(s->time_expr);
            fields["bucket_size"] = Value(s->bucket_size);
            Value::List agg_list;
            for (uint64_t ref : s->agg_refs) {
                agg_list.push_back(Value(ref));
            }
            fields["agg_list"] = Value(std::move(agg_list));
            std::string err;
            if (!emitVNextContractInstruction(stmt->kind(), fields, inst, err)) {
                fail(err);
            }
            return inst;
        }
        case parser::v3::ASTKind::AST_SEARCH_QUERY_DSL: {
            auto* s = static_cast<parser::v3::SearchQueryDslStmt*>(stmt);
            Value::Object fields;
            std::string payload_json;
            if (s->dsl_payload_json != parser::v3::StringPool::INVALID_ID) {
                payload_json = std::string(pool_.get(s->dsl_payload_json));
            }
            fields["dsl_payload_json"] = Value(std::move(payload_json));
            fields["target_index"] = Value(s->target_index);
            fields["scorer_id"] = Value(static_cast<uint64_t>(s->scorer_id));
            std::string err;
            if (!emitVNextContractInstruction(stmt->kind(), fields, inst, err)) {
                fail(err);
            }
            return inst;
        }
        case parser::v3::ASTKind::AST_VECTOR_ANN_QUERY: {
            auto* s = static_cast<parser::v3::VectorAnnQueryStmt*>(stmt);
            Value::Object fields;
            fields["vector_expr"] = Value(s->vector_expr);
            fields["metric"] = Value(static_cast<uint64_t>(s->metric));
            fields["k"] = Value(s->k);
            fields["ef_search"] = Value(s->ef_search);
            std::string err;
            if (!emitVNextContractInstruction(stmt->kind(), fields, inst, err)) {
                fail(err);
            }
            return inst;
        }
        case parser::v3::ASTKind::AST_HYBRID_BRIDGE: {
            auto* s = static_cast<parser::v3::HybridBridgeStmt*>(stmt);
            Value::Object fields;
            fields["source_track"] = Value(s->source_track);
            fields["target_track"] = Value(s->target_track);
            fields["bridge_mode"] = Value(static_cast<uint64_t>(s->bridge_mode));
            std::string err;
            if (!emitVNextContractInstruction(stmt->kind(), fields, inst, err)) {
                fail(err);
            }
            return inst;
        }
        case parser::v3::ASTKind::AST_UDR_COMPILE_DISPATCH: {
            auto* s = static_cast<parser::v3::UdrCompileDispatchStmt*>(stmt);
            auto id_text = [&](parser::v3::StringPool::StringId id) {
                return id == parser::v3::StringPool::INVALID_ID
                           ? std::string()
                           : std::string(pool_.get(id));
            };
            Value::Object fields;
            fields["validate_only"] = Value(static_cast<uint64_t>(s->validate_only ? 1 : 0));
            fields["profile_id"] = Value(id_text(s->profile_id));
            fields["payload_format"] = Value(id_text(s->payload_format));
            fields["payload_bytes"] = Value(id_text(s->payload_bytes));
            fields["session_signature"] = Value(id_text(s->session_signature));
            std::string err;
            if (!emitVNextContractInstruction(stmt->kind(), fields, inst, err)) {
                fail(err);
            }
            return inst;
        }
        case parser::v3::ASTKind::AST_UDR_EMBEDDED_SQL_COMPILE: {
            auto* s = static_cast<parser::v3::UdrEmbeddedSqlCompileStmt*>(stmt);
            auto id_text = [&](parser::v3::StringPool::StringId id) {
                return id == parser::v3::StringPool::INVALID_ID
                           ? std::string()
                           : std::string(pool_.get(id));
            };
            Value::Object fields;
            fields["validate_only"] = Value(static_cast<uint64_t>(s->validate_only ? 1 : 0));
            fields["template_id"] = Value(id_text(s->template_id));
            fields["sql_text"] = Value(id_text(s->sql_text));
            fields["profile_id"] = Value(id_text(s->profile_id));
            fields["session_signature"] = Value(id_text(s->session_signature));
            std::string err;
            if (!emitVNextContractInstruction(stmt->kind(), fields, inst, err)) {
                fail(err);
            }
            return inst;
        }
        default:
            break;
    }

    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitPsql(parser::v3::Statement* stmt) {
    Instruction inst;
    inst.flags = 0;

    switch (stmt->kind()) {
        case parser::v3::ASTKind::ExecuteBlockStmt: {
            auto* s = static_cast<parser::v3::ExecuteBlockStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_BLOCK);
            Value::Object payload;
            Value::List decls;
            for (const auto& decl : s->input_params) {
                Value::Object d;
                d["name"] = toIdent(decl.name);
                d["type"] = Value(buildTypeSpec(decl.type));
                d["constant"] = Value(false);
                if (decl.default_value) d["default"] = Value(makeInstr(emitExpression(decl.default_value)));
                decls.push_back(Value(std::move(d)));
            }
            for (const auto& decl : s->output_params) {
                Value::Object d;
                d["name"] = toIdent(decl.name);
                d["type"] = Value(buildTypeSpec(decl.type));
                d["constant"] = Value(false);
                decls.push_back(Value(std::move(d)));
            }
            for (const auto& decl : s->variables) {
                Value::Object d;
                d["name"] = toIdent(decl.name);
                d["type"] = Value(buildTypeSpec(decl.type));
                d["constant"] = Value(false);
                if (decl.default_value) d["default"] = Value(makeInstr(emitExpression(decl.default_value)));
                decls.push_back(Value(std::move(d)));
            }
            payload["decls"] = Value(std::move(decls));
            payload["body"] = s->body ? toStmtList({s->body}) : Value(Value::List{});
            payload["exception_handlers"] = Value(Value::List{});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CompoundStmt: {
            auto* s = static_cast<parser::v3::CompoundStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_BLOCK);
            Value::Object payload;
            payload["decls"] = Value(Value::List{});
            payload["body"] = toStmtList(s->statements);
            Value::List handlers;
            for (auto* h : s->exception_handlers) {
                if (!h || h->kind() != parser::v3::ASTKind::WhenExceptionStmt) continue;
                auto* wh = static_cast<parser::v3::WhenExceptionStmt*>(h);
                Value::Object ex;
                std::string cond = "ANY";
                if (wh->type == parser::v3::WhenExceptionStmt::ExceptionType::SQLCODE) {
                    cond = "SQLCODE " + std::to_string(wh->sqlcode);
                } else if (wh->type == parser::v3::WhenExceptionStmt::ExceptionType::GDSCODE) {
                    cond = std::string(pool_.get(wh->gdscode));
                } else if (wh->type == parser::v3::WhenExceptionStmt::ExceptionType::EXCEPTION) {
                    cond = std::string(pool_.get(wh->exception_name));
                }
                ex["condition"] = Value(cond);
                if (wh->handler) {
                    ex["handler"] = toStmtList({wh->handler});
                }
                handlers.push_back(Value(std::move(ex)));
            }
            payload["exception_handlers"] = Value(std::move(handlers));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::DeclareVariableStmt: {
            auto* s = static_cast<parser::v3::DeclareVariableStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_DECLARE);
            Value::Object decl;
            decl["name"] = toIdent(s->name);
            decl["type"] = Value(buildTypeSpec(s->type));
            decl["constant"] = Value(false);
            if (s->default_value) decl["default"] = Value(makeInstr(emitExpression(s->default_value)));
            Value::Object payload;
            payload["decls"] = Value(Value::List{Value(std::move(decl))});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AssignmentStmt: {
            auto* s = static_cast<parser::v3::AssignmentStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_ASSIGN);
            Value::Object payload;
            payload["target"] = emitVarRefValue(s->variable);
            payload["value"] = Value(makeInstr(emitExpression(s->value)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::IfStmt: {
            auto* s = static_cast<parser::v3::IfStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_IF);
            Value::Object payload;
            payload["condition"] = Value(makeInstr(emitExpression(s->condition)));
            payload["then_body"] = toStmtList({s->then_branch});
            Value::List elsif;
            if (s->else_branch && s->else_branch->kind() == parser::v3::ASTKind::IfStmt) {
                auto* else_if = static_cast<parser::v3::IfStmt*>(s->else_branch);
                Value::Object e;
                e["condition"] = Value(makeInstr(emitExpression(else_if->condition)));
                e["body"] = toStmtList({else_if->then_branch});
                elsif.push_back(Value(std::move(e)));
                if (else_if->else_branch) {
                    payload["else_body"] = toStmtList({else_if->else_branch});
                }
            } else if (s->else_branch) {
                payload["else_body"] = toStmtList({s->else_branch});
            }
            payload["elsif"] = Value(std::move(elsif));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::WhileStmt: {
            auto* s = static_cast<parser::v3::WhileStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_WHILE);
            Value::Object payload;
            payload["condition"] = Value(makeInstr(emitExpression(s->condition)));
            payload["body"] = toStmtList({s->body});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::ForSelectStmt: {
            auto* s = static_cast<parser::v3::ForSelectStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_FOR_SELECT);
            Value::Object payload;
            if (!s->into_variables.empty()) {
                payload["record"] = emitVarRefValue(s->into_variables.front());
            } else {
                payload["record"] = emitVarRefValue(parser::v3::StringPool::INVALID_ID);
            }
            if (s->select_stmt) {
                payload["query"] = Value(makeInstr(emitStatement(s->select_stmt)));
            }
            payload["body"] = toStmtList({s->body});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::ForExecuteStmt: {
            auto* s = static_cast<parser::v3::ForExecuteStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_FOR_EXECUTE);
            Value::Object payload;
            if (!s->into_variables.empty()) {
                payload["record"] = emitVarRefValue(s->into_variables.front());
            } else {
                payload["record"] = emitVarRefValue(parser::v3::StringPool::INVALID_ID);
            }
            if (s->sql) payload["sql"] = Value(makeInstr(emitExpression(s->sql)));
            payload["body"] = toStmtList({s->body});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LoopStmt: {
            auto* s = static_cast<parser::v3::LoopStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_LOOP);
            Value::Object payload;
            payload["body"] = toStmtList({s->body});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::ExitStmt: {
            inst.opcode = op(Opcode::SBLR3_EXIT);
            inst.payload = Value(Value::Object{});
            return inst;
        }
        case parser::v3::ASTKind::LeaveStmt: {
            auto* s = static_cast<parser::v3::LeaveStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_LEAVE);
            Value::Object payload;
            if (s->label != parser::v3::StringPool::INVALID_ID) payload["label"] = toIdent(s->label);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::ContinueStmt: {
            auto* s = static_cast<parser::v3::ContinueStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_CONTINUE);
            Value::Object payload;
            if (s->label != parser::v3::StringPool::INVALID_ID) payload["label"] = toIdent(s->label);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::SuspendStmt:
            inst.opcode = op(Opcode::SBLR3_SUSPEND);
            inst.payload = Value(Value::Object{});
            return inst;
        case parser::v3::ASTKind::ReturnStmt: {
            auto* s = static_cast<parser::v3::ReturnStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_RETURN);
            Value::Object payload;
            if (s->value) payload["value"] = Value(makeInstr(emitExpression(s->value)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::ExceptionRaiseStmt: {
            auto* s = static_cast<parser::v3::ExceptionRaiseStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_RAISE);
            Value::Object payload;
            if (s->exception_name != parser::v3::StringPool::INVALID_ID) {
                payload["message"] = Value(std::string(pool_.get(s->exception_name)));
            }
            if (s->message) payload["params"] = Value(Value::List{Value(makeInstr(emitExpression(s->message)))});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::PostEventStmt: {
            auto* s = static_cast<parser::v3::PostEventStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_POST_EVENT);
            Value::Object payload;
            if (s->event_name && s->event_name->kind() == parser::v3::ASTKind::LiteralExpr) {
                auto* lit = static_cast<parser::v3::LiteralExpr*>(s->event_name);
                if (lit->literal_type == parser::v3::LiteralType::STRING) {
                    payload["event_name"] = Value(std::string(pool_.get(lit->string_value)));
                } else {
                    fail("POST_EVENT requires a string literal event name");
                }
            } else if (s->event_name) {
                fail("POST_EVENT requires a string literal event name");
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::ExecuteProcedureStmt: {
            auto* s = static_cast<parser::v3::ExecuteProcedureStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CALL);
            Value::Object payload;
            payload["proc_name"] = toIdent(s->procedure_path.objectName());
            payload["args"] = toExprList(s->arguments);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::ExecuteStatementStmt: {
            auto* s = static_cast<parser::v3::ExecuteStatementStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_PSQL_FOR_EXECUTE);
            Value::Object payload;
            if (!s->into_variables.empty()) {
                payload["record"] = emitVarRefValue(s->into_variables.front());
            } else {
                payload["record"] = emitVarRefValue(parser::v3::StringPool::INVALID_ID);
            }
            if (s->sql) payload["sql"] = Value(makeInstr(emitExpression(s->sql)));
            if (s->external_data_source) {
                payload["external_data_source"] = Value(makeInstr(emitExpression(s->external_data_source)));
            }
            if (s->as_user) {
                payload["as_user"] = Value(makeInstr(emitExpression(s->as_user)));
            }
            if (s->password) {
                payload["password"] = Value(makeInstr(emitExpression(s->password)));
            }
            if (s->role) {
                payload["role"] = Value(makeInstr(emitExpression(s->role)));
            }
            payload["with_autonomous_transaction"] = Value(s->with_autonomous_transaction);
            payload["with_common_transaction"] = Value(s->with_common_transaction);
            payload["with_caller_privileges"] = Value(s->with_caller_privileges);
            payload["body"] = Value(Value::List{});
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::DeclareCursorStmt: {
            auto* s = static_cast<parser::v3::DeclareCursorStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CURSOR_DECLARE);
            Value::Object payload;
            payload["cursor_name"] = toIdent(s->cursor_name);
            payload["scroll"] = Value(s->scroll);
            if (s->select_stmt) payload["query"] = Value(makeInstr(emitStatement(s->select_stmt)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::OpenCursorStmt: {
            auto* s = static_cast<parser::v3::OpenCursorStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CURSOR_OPEN);
            Value::Object payload;
            payload["cursor_name"] = toIdent(s->cursor_name);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::FetchCursorStmt: {
            auto* s = static_cast<parser::v3::FetchCursorStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CURSOR_FETCH);
            Value::Object payload;
            payload["cursor_name"] = toIdent(s->cursor_name);
            payload["direction"] = Value(uint64_t(static_cast<uint8_t>(s->direction)));
            if (s->offset) payload["offset"] = Value(makeInstr(emitExpression(s->offset)));
            if (!s->into_variables.empty()) {
                payload["target"] = emitVarRefValue(s->into_variables.front());
            }
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CloseCursorStmt: {
            auto* s = static_cast<parser::v3::CloseCursorStmt*>(stmt);
            inst.opcode = op(Opcode::SBLR3_CURSOR_CLOSE);
            Value::Object payload;
            payload["cursor_name"] = toIdent(s->cursor_name);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }
    inst.opcode = op(Opcode::SBLR3_EXECUTE_STMT);
    inst.payload = Value(Value::Bytes{});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitExpression(parser::v3::Expression* expr) {
    if (!expr) {
        Instruction inst;
        inst.opcode = op(Opcode::SBLR3_LITERAL_NULL);
        inst.flags = 0;
        inst.payload = Value(Value::Object{{"value", Value() }});
        return inst;
    }
    auto makeStringLiteral = [&](const std::string& text) {
        Instruction lit;
        lit.opcode = op(Opcode::SBLR3_LITERAL_STRING);
        lit.flags = 0;
        lit.payload = Value(Value::Object{{"value", Value(text)}});
        return lit;
    };
    auto selectorToInstr = [&](const parser::v3::ElementSelector& selector) {
        switch (selector.kind) {
            case parser::v3::ElementSelector::Kind::IDENTIFIER:
                return makeStringLiteral(std::string(pool_.get(selector.identifier)));
            case parser::v3::ElementSelector::Kind::STRING_LITERAL:
                return makeStringLiteral(std::string(pool_.get(selector.string_literal)));
            case parser::v3::ElementSelector::Kind::INTEGER_EXPR:
                return emitExpression(selector.expr);
        }
        return makeStringLiteral(std::string());
    };
    auto encodeExprBytes = [&](parser::v3::Expression* value) -> Value::Bytes {
        if (!value) {
            return encodeInstructionBytes(emitLiteral(nullptr));
        }
        return encodeInstructionBytes(emitExpression(value));
    };
    auto appendU128 = [](const parser::v3::U128& v, Value::Bytes& out) {
        out.insert(out.end(), v.begin(), v.end());
    };
    switch (expr->kind()) {
        case parser::v3::ASTKind::LiteralExpr:
            return emitLiteral(static_cast<parser::v3::LiteralExpr*>(expr));
        case parser::v3::ASTKind::LiteralEnumExpr: {
            auto* s = static_cast<parser::v3::LiteralEnumExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_ENUM);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->enum_catalog_id, bytes);
            uint8_t flags = 0;
            if (s->has_label) flags |= 0x01;
            if (s->has_ordinal) flags |= 0x02;
            bytes.push_back(flags);
            if (s->has_label && s->label != parser::v3::StringPool::INVALID_ID) {
                appendStringWithLen(std::string(pool_.get(s->label)), bytes);
            } else {
                appendVaruint(0, bytes);
            }
            if (s->has_ordinal) {
                appendLE32(static_cast<uint32_t>(s->ordinal), bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralSetExpr: {
            auto* s = static_cast<parser::v3::LiteralSetExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_SET);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->set_catalog_id, bytes);
            appendVaruint(s->elements.size(), bytes);
            for (auto* elem : s->elements) {
                Value::Bytes ebytes;
                if (elem) {
                    appendU128(elem->enum_catalog_id, ebytes);
                    uint8_t flags = 0;
                    if (elem->has_label) flags |= 0x01;
                    if (elem->has_ordinal) flags |= 0x02;
                    ebytes.push_back(flags);
                    if (elem->has_label && elem->label != parser::v3::StringPool::INVALID_ID) {
                        appendStringWithLen(std::string(pool_.get(elem->label)), ebytes);
                    } else {
                        appendVaruint(0, ebytes);
                    }
                    if (elem->has_ordinal) {
                        appendLE32(static_cast<uint32_t>(elem->ordinal), ebytes);
                    }
                }
                appendBytesWithLen(ebytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralRowExpr: {
            auto* s = static_cast<parser::v3::LiteralRowExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_ROW);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->row_catalog_id, bytes);
            appendVaruint(s->fields.size(), bytes);
            for (const auto& field : s->fields) {
                if (field.name != parser::v3::StringPool::INVALID_ID) {
                    appendStringWithLen(std::string(pool_.get(field.name)), bytes);
                } else {
                    appendVaruint(0, bytes);
                }
                Value::Bytes vbytes = encodeExprBytes(field.value);
                appendBytesWithLen(vbytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralCompositeExpr: {
            auto* s = static_cast<parser::v3::LiteralCompositeExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_COMPOSITE);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->composite_catalog_id, bytes);
            appendVaruint(s->fields.size(), bytes);
            for (const auto& field : s->fields) {
                if (field.name != parser::v3::StringPool::INVALID_ID) {
                    appendStringWithLen(std::string(pool_.get(field.name)), bytes);
                } else {
                    appendVaruint(0, bytes);
                }
                Value::Bytes vbytes = encodeExprBytes(field.value);
                appendBytesWithLen(vbytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralDomainExpr: {
            auto* s = static_cast<parser::v3::LiteralDomainExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_DOMAIN);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->domain_id, bytes);
            Value::Bytes vbytes = encodeExprBytes(s->value);
            appendBytesWithLen(vbytes, bytes);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralBitExpr: {
            auto* s = static_cast<parser::v3::LiteralBitExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_BIT);
            inst.flags = 0;
            Value::Bytes bytes;
            appendLE16(s->bit_length, bytes);
            bytes.insert(bytes.end(), s->bytes.begin(), s->bytes.end());
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralYearExpr: {
            auto* s = static_cast<parser::v3::LiteralYearExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_YEAR);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralDateTimeExpr: {
            auto* s = static_cast<parser::v3::LiteralDateTimeExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_DATETIME);
            inst.flags = 0;
            Value::Bytes bytes;
            appendLE64(static_cast<uint64_t>(s->epoch_usec), bytes);
            bytes.push_back(s->with_timezone ? 1 : 0);
            bytes.push_back(s->precision);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralMediumIntExpr: {
            auto* s = static_cast<parser::v3::LiteralMediumIntExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_MEDIUMINT);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralGeometryExpr: {
            auto* s = static_cast<parser::v3::LiteralGeometryExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_GEOMETRY);
            inst.flags = 0;
            Value::Bytes bytes;
            bytes.push_back(s->format);
            appendLE32(s->srid, bytes);
            bytes.insert(bytes.end(), s->bytes.begin(), s->bytes.end());
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralJsonPathExpr: {
            auto* s = static_cast<parser::v3::LiteralJsonPathExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_JSONPATH);
            inst.flags = 0;
            Value::Bytes bytes;
            bytes.push_back(s->dialect);
            if (s->text != parser::v3::StringPool::INVALID_ID) {
                appendStringWithLen(std::string(pool_.get(s->text)), bytes);
            } else {
                appendVaruint(0, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralInt8Expr: {
            auto* s = static_cast<parser::v3::LiteralInt8Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_INT8);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralInt16Expr: {
            auto* s = static_cast<parser::v3::LiteralInt16Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_INT16);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralUInt8Expr: {
            auto* s = static_cast<parser::v3::LiteralUInt8Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT8);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(uint64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralUInt16Expr: {
            auto* s = static_cast<parser::v3::LiteralUInt16Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT16);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(uint64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralUInt32Expr: {
            auto* s = static_cast<parser::v3::LiteralUInt32Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT32);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(uint64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralUInt64Expr: {
            auto* s = static_cast<parser::v3::LiteralUInt64Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT64);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(uint64_t(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralUInt128Expr: {
            auto* s = static_cast<parser::v3::LiteralUInt128Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_UINT128);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->value, bytes);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralInt128Expr: {
            auto* s = static_cast<parser::v3::LiteralInt128Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_INT128);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->value, bytes);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralFloat32Expr: {
            auto* s = static_cast<parser::v3::LiteralFloat32Expr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_FLOAT32);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(static_cast<double>(s->value));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralTimeTzExpr: {
            auto* s = static_cast<parser::v3::LiteralTimeTzExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_TIME_TZ);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->time_usec));
            payload["offset_seconds"] = Value(int64_t(s->tz_offset_minutes) * 60);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralTimestampTzExpr: {
            auto* s = static_cast<parser::v3::LiteralTimestampTzExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_TIMESTAMP_TZ);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(int64_t(s->epoch_usec));
            payload["offset_seconds"] = Value(int64_t(s->tz_offset_minutes) * 60);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::LiteralRangeExpr: {
            auto* s = static_cast<parser::v3::LiteralRangeExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_RANGE);
            inst.flags = 0;
            Value::Bytes bytes;
            TypeSpec spec = buildTypeSpec(s->range_base_type);
            appendLE16(spec.type_opcode, bytes);
            appendLE32(static_cast<uint32_t>(spec.type_payload.size()), bytes);
            bytes.insert(bytes.end(), spec.type_payload.begin(), spec.type_payload.end());
            bytes.push_back(s->flags);
            bytes.push_back(s->lower_present ? 1 : 0);
            bytes.push_back(s->upper_present ? 1 : 0);
            if (s->lower_present) {
                Value::Bytes lower_bytes = encodeExprBytes(s->lower);
                appendBytesWithLen(lower_bytes, bytes);
            }
            if (s->upper_present) {
                Value::Bytes upper_bytes = encodeExprBytes(s->upper);
                appendBytesWithLen(upper_bytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralArrayExpr: {
            auto* s = static_cast<parser::v3::LiteralArrayExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_ARRAY);
            inst.flags = 0;
            Value::Bytes bytes;
            TypeSpec spec = buildTypeSpec(s->element_type);
            appendLE16(spec.type_opcode, bytes);
            appendLE32(static_cast<uint32_t>(spec.type_payload.size()), bytes);
            bytes.insert(bytes.end(), spec.type_payload.begin(), spec.type_payload.end());
            bytes.push_back(s->dimensions);
            appendVaruint(s->dim_lengths.size(), bytes);
            for (auto len : s->dim_lengths) {
                appendLE32(len, bytes);
            }
            appendVaruint(s->elements.size(), bytes);
            for (auto* elem : s->elements) {
                Value::Bytes ebytes = encodeExprBytes(elem);
                appendBytesWithLen(ebytes, bytes);
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralVariantExpr: {
            auto* s = static_cast<parser::v3::LiteralVariantExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_VARIANT);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->variant_type_id, bytes);
            if (s->tag_name != parser::v3::StringPool::INVALID_ID) {
                appendStringWithLen(std::string(pool_.get(s->tag_name)), bytes);
            } else {
                appendVaruint(0, bytes);
            }
            Value::Bytes vbytes = encodeExprBytes(s->value);
            appendBytesWithLen(vbytes, bytes);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralTsVectorExpr: {
            auto* s = static_cast<parser::v3::LiteralTsVectorExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_TSVECTOR);
            inst.flags = 0;
            Value::Bytes bytes;
            if (s->text != parser::v3::StringPool::INVALID_ID) {
                auto text = std::string(pool_.get(s->text));
                bytes.assign(text.begin(), text.end());
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralTsQueryExpr: {
            auto* s = static_cast<parser::v3::LiteralTsQueryExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_TSQUERY);
            inst.flags = 0;
            Value::Bytes bytes;
            if (s->text != parser::v3::StringPool::INVALID_ID) {
                auto text = std::string(pool_.get(s->text));
                bytes.assign(text.begin(), text.end());
            }
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::LiteralBlobLocatorExpr: {
            auto* s = static_cast<parser::v3::LiteralBlobLocatorExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_LITERAL_BLOB_LOCATOR);
            inst.flags = 0;
            Value::Bytes bytes;
            appendU128(s->blob_id, bytes);
            appendLE16(static_cast<uint16_t>(s->blob_subtype), bytes);
            appendLE64(static_cast<uint64_t>(s->blob_length), bytes);
            bytes.push_back(s->compression);
            inst.payload = Value(Value::Object{{"value", Value(std::move(bytes))}});
            return inst;
        }
        case parser::v3::ASTKind::ColumnRefExpr:
            return emitColumnRef(static_cast<parser::v3::ColumnRefExpr*>(expr));
        case parser::v3::ASTKind::ParameterExpr: {
            auto* p = static_cast<parser::v3::ParameterExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_VAR_LOAD);
            inst.flags = 0;
            std::string name;
            if (p->is_named && p->name != parser::v3::StringPool::INVALID_ID) {
                name = std::string(pool_.get(p->name));
            } else {
                name = "$" + std::to_string(p->index);
            }
            Value::Object var;
            var["name"] = Value(name);
            Value::Object payload;
            payload["var"] = Value(std::move(var));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::BinaryExpr:
            return emitBinary(static_cast<parser::v3::BinaryExpr*>(expr));
        case parser::v3::ASTKind::UnaryExpr:
            return emitUnary(static_cast<parser::v3::UnaryExpr*>(expr));
        case parser::v3::ASTKind::FunctionCallExpr:
            return emitFunctionCall(static_cast<parser::v3::FunctionCallExpr*>(expr));
        case parser::v3::ASTKind::CastExpr:
            return emitCast(static_cast<parser::v3::CastExpr*>(expr));
        case parser::v3::ASTKind::ExtractExpr: {
            auto* e = static_cast<parser::v3::ExtractExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXTRACT);
            inst.flags = 0;
            Value::Object payload;
            Value::List args;
            args.push_back(Value(makeInstr(selectorToInstr(e->selector))));
            for (auto* arg : e->selector.args) {
                args.push_back(Value(makeInstr(emitExpression(arg))));
            }
            if (e->source) args.push_back(Value(makeInstr(emitExpression(e->source))));
            payload["args"] = Value(std::move(args));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::AlterElementExpr: {
            auto* e = static_cast<parser::v3::AlterElementExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_ALTER_ELEMENT);
            inst.flags = 0;
            Value::Object payload;
            Value::List args;
            args.push_back(Value(makeInstr(selectorToInstr(e->selector))));
            for (auto* arg : e->selector.args) {
                args.push_back(Value(makeInstr(emitExpression(arg))));
            }
            if (e->source) args.push_back(Value(makeInstr(emitExpression(e->source))));
            if (e->new_value) args.push_back(Value(makeInstr(emitExpression(e->new_value))));
            payload["args"] = Value(std::move(args));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::ASTKind::CaseExpr:
            return emitCase(static_cast<parser::v3::CaseExpr*>(expr));
        case parser::v3::ASTKind::InExpr:
            return emitIn(static_cast<parser::v3::InExpr*>(expr));
        case parser::v3::ASTKind::BetweenExpr:
            return emitBetween(static_cast<parser::v3::BetweenExpr*>(expr));
        case parser::v3::ASTKind::LikeExpr:
            return emitLike(static_cast<parser::v3::LikeExpr*>(expr));
        case parser::v3::ASTKind::ExistsExpr:
            return emitExists(static_cast<parser::v3::ExistsExpr*>(expr));
        case parser::v3::ASTKind::SubqueryExpr:
            return emitSubquery(static_cast<parser::v3::SubqueryExpr*>(expr));
        case parser::v3::ASTKind::IsNullExpr: {
            auto* s = static_cast<parser::v3::IsNullExpr*>(expr);
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXPR_IS_NULL);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(makeInstr(emitExpression(s->expr)));
            inst.payload = Value(std::move(payload));
            if (s->negated) {
                Instruction not_inst;
                not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
                not_inst.flags = 0;
                Value::Object not_payload;
                not_payload["value"] = Value(makeInstr(inst));
                not_inst.payload = Value(std::move(not_payload));
                return not_inst;
            }
            return inst;
        }
        case parser::v3::ASTKind::ArrayExpr: {
            auto* s = static_cast<parser::v3::ArrayExpr*>(expr);
            Instruction inst;
            if (s->has_subquery && s->subquery) {
                inst.opcode = op(Opcode::SBLR3_SUBQUERY_ARRAY);
                inst.flags = 0;
                Value::Object payload;
                payload["query"] = Value(makeInstr(emitSelect(s->subquery)));
                inst.payload = Value(std::move(payload));
                return inst;
            }
            inst.opcode = op(Opcode::SBLR3_ARRAY_CONSTRUCT);
            inst.flags = 0;
            Value::Object payload;
            payload["args"] = toExprList(s->elements);
            inst.payload = Value(std::move(payload));
            return inst;
        }
        default:
            break;
    }
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_LITERAL_NULL);
    inst.flags = 0;
    inst.payload = Value(Value::Object{{"value", Value()}});
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitLiteral(parser::v3::LiteralExpr* lit) {
    Instruction inst;
    inst.flags = 0;
    Value::Object payload;
    if (!lit) {
        inst.opcode = op(Opcode::SBLR3_LITERAL_NULL);
        payload["value"] = Value();
        inst.payload = Value(std::move(payload));
        return inst;
    }
    switch (lit->literal_type) {
        case parser::v3::LiteralType::INTEGER:
            inst.opcode = op(Opcode::SBLR3_LITERAL_INT64);
            payload["value"] = Value(int64_t(lit->int_value));
            break;
        case parser::v3::LiteralType::FLOAT:
            inst.opcode = op(Opcode::SBLR3_LITERAL_DOUBLE);
            payload["value"] = Value(double(lit->float_value));
            break;
        case parser::v3::LiteralType::STRING:
            inst.opcode = op(Opcode::SBLR3_LITERAL_STRING);
            payload["value"] = Value(std::string(pool_.get(lit->string_value)));
            break;
        case parser::v3::LiteralType::BLOB:
            inst.opcode = op(Opcode::SBLR3_LITERAL_BINARY);
            {
                std::string s(pool_.get(lit->string_value));
                Value::Bytes b(s.begin(), s.end());
                payload["value"] = Value(std::move(b));
            }
            break;
        case parser::v3::LiteralType::BOOLEAN:
            inst.opcode = op(Opcode::SBLR3_LITERAL_BOOLEAN);
            payload["value"] = Value(lit->bool_value);
            break;
        case parser::v3::LiteralType::NULL_VALUE:
            inst.opcode = op(Opcode::SBLR3_LITERAL_NULL);
            payload["value"] = Value();
            break;
        case parser::v3::LiteralType::DEFAULT:
            inst.opcode = op(Opcode::SBLR3_DEFAULT_VALUE);
            inst.payload = Value(Value::Bytes{});
            return inst;
    }
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitColumnRef(parser::v3::ColumnRefExpr* ref) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_COLUMN_REF);
    inst.flags = 0;
    Value::Object payload;
    if (ref->column.has_table_qualifier) {
        payload["path"] = toSchemaPath(ref->column.table_path);
    } else {
        payload["path"] = Value(Value::List{});
    }
    payload["column"] = toIdent(ref->column.column_name);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitBinary(parser::v3::BinaryExpr* expr) {
    if (!expr) {
        return emitLiteral(nullptr);
    }
    Instruction inst;
    inst.flags = 0;
    bool mapped = true;
    Value::Object payload;
    payload["lhs"] = Value(makeInstr(emitExpression(expr->left)));
    payload["rhs"] = Value(makeInstr(emitExpression(expr->right)));

    switch (expr->op) {
        case parser::v3::BinaryOp::ADD: inst.opcode = op(Opcode::SBLR3_EXPR_ADD); break;
        case parser::v3::BinaryOp::SUB: inst.opcode = op(Opcode::SBLR3_EXPR_SUBTRACT); break;
        case parser::v3::BinaryOp::MUL: inst.opcode = op(Opcode::SBLR3_EXPR_MULTIPLY); break;
        case parser::v3::BinaryOp::DIV: inst.opcode = op(Opcode::SBLR3_EXPR_DIVIDE); break;
        case parser::v3::BinaryOp::DIV_INT: inst.opcode = op(Opcode::SBLR3_EXPR_DIV_INT); break;
        case parser::v3::BinaryOp::MOD: inst.opcode = op(Opcode::SBLR3_EXPR_MODULO); break;
        case parser::v3::BinaryOp::POWER: {
            inst.opcode = op(Opcode::SBLR3_FUNC_POWER);
            Value::Object f;
            Value::List args;
            args.push_back(Value(makeInstr(emitExpression(expr->left))));
            args.push_back(Value(makeInstr(emitExpression(expr->right))));
            f["args"] = Value(std::move(args));
            inst.payload = Value(std::move(f));
            return inst;
        }
        case parser::v3::BinaryOp::EQ: inst.opcode = op(Opcode::SBLR3_EXPR_EQ); break;
        case parser::v3::BinaryOp::NE: inst.opcode = op(Opcode::SBLR3_EXPR_NE); break;
        case parser::v3::BinaryOp::LT: inst.opcode = op(Opcode::SBLR3_EXPR_LT); break;
        case parser::v3::BinaryOp::LE: inst.opcode = op(Opcode::SBLR3_EXPR_LE); break;
        case parser::v3::BinaryOp::GT: inst.opcode = op(Opcode::SBLR3_EXPR_GT); break;
        case parser::v3::BinaryOp::GE: inst.opcode = op(Opcode::SBLR3_EXPR_GE); break;
        case parser::v3::BinaryOp::NULL_SAFE_EQ: inst.opcode = op(Opcode::SBLR3_NULL_SAFE_EQ); break;
        case parser::v3::BinaryOp::AND: inst.opcode = op(Opcode::SBLR3_EXPR_AND); break;
        case parser::v3::BinaryOp::OR: inst.opcode = op(Opcode::SBLR3_EXPR_OR); break;
        case parser::v3::BinaryOp::BIT_AND: inst.opcode = op(Opcode::SBLR3_BIT_AND); break;
        case parser::v3::BinaryOp::BIT_OR: inst.opcode = op(Opcode::SBLR3_BIT_OR); break;
        case parser::v3::BinaryOp::BIT_XOR: inst.opcode = op(Opcode::SBLR3_BIT_XOR); break;
        case parser::v3::BinaryOp::SHIFT_LEFT: inst.opcode = op(Opcode::SBLR3_BIT_SHIFT_LEFT); break;
        case parser::v3::BinaryOp::SHIFT_RIGHT: inst.opcode = op(Opcode::SBLR3_BIT_SHIFT_RIGHT); break;
        case parser::v3::BinaryOp::REGEX_MATCH: inst.opcode = op(Opcode::SBLR3_REGEX_MATCH); break;
        case parser::v3::BinaryOp::REGEX_MATCH_CI: inst.opcode = op(Opcode::SBLR3_REGEX_MATCH_CI); break;
        case parser::v3::BinaryOp::REGEX_NOT_MATCH: inst.opcode = op(Opcode::SBLR3_REGEX_NOT_MATCH); break;
        case parser::v3::BinaryOp::REGEX_NOT_MATCH_CI: inst.opcode = op(Opcode::SBLR3_REGEX_NOT_MATCH_CI); break;
        case parser::v3::BinaryOp::JSON_EXTRACT: inst.opcode = op(Opcode::SBLR3_JSON_EXTRACT); break;
        case parser::v3::BinaryOp::JSON_EXTRACT_TEXT: inst.opcode = op(Opcode::SBLR3_JSON_DOUBLE_ARROW); break;
        case parser::v3::BinaryOp::JSON_HASH_EXTRACT: inst.opcode = op(Opcode::SBLR3_JSON_HASH_ARROW); break;
        case parser::v3::BinaryOp::JSON_HASH_EXTRACT_TEXT: inst.opcode = op(Opcode::SBLR3_JSON_HASH_DOUBLE_ARROW); break;
        case parser::v3::BinaryOp::JSON_EXISTS:
        case parser::v3::BinaryOp::JSON_EXISTS_ANY:
        case parser::v3::BinaryOp::JSON_EXISTS_ALL: {
            inst.opcode = op(Opcode::SBLR3_FUNC_JSON_EXISTS);
            if (expr->op == parser::v3::BinaryOp::JSON_EXISTS_ANY) {
                inst.flags |= kFuncJsonExistsAnyFlag;
            } else if (expr->op == parser::v3::BinaryOp::JSON_EXISTS_ALL) {
                inst.flags |= kFuncJsonExistsAllFlag;
            }
            Value::Object f;
            Value::List args;
            args.push_back(Value(makeInstr(emitExpression(expr->left))));
            args.push_back(Value(makeInstr(emitExpression(expr->right))));
            f["args"] = Value(std::move(args));
            inst.payload = Value(std::move(f));
            return inst;
        }
        case parser::v3::BinaryOp::ARRAY_CONTAINS: inst.opcode = op(Opcode::SBLR3_ARRAY_CONTAINS); break;
        case parser::v3::BinaryOp::ARRAY_CONTAINED_BY: inst.opcode = op(Opcode::SBLR3_ARRAY_CONTAINED_BY); break;
        case parser::v3::BinaryOp::ARRAY_OVERLAP: inst.opcode = op(Opcode::SBLR3_ARRAY_OVERLAP); break;
        case parser::v3::BinaryOp::CONCAT: {
            inst.opcode = op(Opcode::SBLR3_FUNC_CONCAT);
            Value::Object f;
            Value::List args;
            args.push_back(Value(makeInstr(emitExpression(expr->left))));
            args.push_back(Value(makeInstr(emitExpression(expr->right))));
            f["args"] = Value(std::move(args));
            inst.payload = Value(std::move(f));
            return inst;
        }
        default:
            mapped = false;
            break;
    }
    if (!mapped) {
        return emitLiteral(nullptr);
    }
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitUnary(parser::v3::UnaryExpr* expr) {
    if (!expr) return emitLiteral(nullptr);
    switch (expr->op) {
        case parser::v3::UnaryOp::NOT: {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(makeInstr(emitExpression(expr->operand)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::UnaryOp::BIT_NOT: {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_BIT_NOT);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(makeInstr(emitExpression(expr->operand)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::UnaryOp::NEGATE: {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXPR_SUBTRACT);
            inst.flags = 0;
            Value::Object payload;
            payload["lhs"] = Value(makeInstr(emitLiteralZero()));
            payload["rhs"] = Value(makeInstr(emitExpression(expr->operand)));
            inst.payload = Value(std::move(payload));
            return inst;
        }
        case parser::v3::UnaryOp::IS_NULL:
        case parser::v3::UnaryOp::IS_NOT_NULL: {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_EXPR_IS_NULL);
            inst.flags = 0;
            Value::Object payload;
            payload["value"] = Value(makeInstr(emitExpression(expr->operand)));
            inst.payload = Value(std::move(payload));
            if (expr->op == parser::v3::UnaryOp::IS_NOT_NULL) {
                Instruction not_inst;
                not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
                not_inst.flags = 0;
                Value::Object not_payload;
                not_payload["value"] = Value(makeInstr(inst));
                not_inst.payload = Value(std::move(not_payload));
                return not_inst;
            }
            return inst;
        }
    }
    return emitLiteral(nullptr);
}

scratchbird::sblr::v3::Instruction V3Emitter::emitFunctionCall(parser::v3::FunctionCallExpr* expr) {
    Instruction inst;
    inst.flags = 0;

    std::string name = toUpper(pool_.get(expr->function_path.objectName()));
    const bool current_timestamp_semantics = (name == "CURRENT_TIMESTAMP");
    static const std::unordered_map<std::string, Opcode> kFuncMap = {
        {"COALESCE", Opcode::SBLR3_COALESCE},
        {"NULLIF", Opcode::SBLR3_NULLIF},
        {"POWER", Opcode::SBLR3_FUNC_POWER},
        {"ABS", Opcode::SBLR3_FUNC_ABS},
        {"SIN", Opcode::SBLR3_FUNC_SIN},
        {"COS", Opcode::SBLR3_FUNC_COS},
        {"TAN", Opcode::SBLR3_FUNC_TAN},
        {"CONCAT", Opcode::SBLR3_FUNC_CONCAT},
        {"NOW", Opcode::SBLR3_FUNC_NOW},
        {"CURRENT_TIMESTAMP", Opcode::SBLR3_FUNC_NOW},
        {"CURRENT_DATE", Opcode::SBLR3_FUNC_CURRENT_DATE},
        {"CURRENT_TIME", Opcode::SBLR3_FUNC_CURRENT_TIME},
        {"CURRENT_USER", Opcode::SBLR3_FUNC_CURRENT_USER},
        {"SESSION_USER", Opcode::SBLR3_FUNC_CURRENT_USER},
        {"CURRENT_ROLE", Opcode::SBLR3_FUNC_CURRENT_ROLE},
        {"CURRENT_CONNECTION", Opcode::SBLR3_FUNC_CURRENT_CONNECTION},
        {"CURRENT_SESSION", Opcode::SBLR3_FUNC_CURRENT_CONNECTION},
        {"CURRENT_TRANSACTION", Opcode::SBLR3_FUNC_CURRENT_TRANSACTION},
        {"ARRAY_POSITION", Opcode::SBLR3_FUNC_ARRAY_POSITION},
        {"ARRAY_SLICE", Opcode::SBLR3_ARRAY_SLICE},
        {"ARRAY_SUBSCRIPT", Opcode::SBLR3_ARRAY_SUBSCRIPT},
        {"REPLACE", Opcode::SBLR3_FUNC_REPLACE},
        {"ENDS_WITH", Opcode::SBLR3_FUNC_ENDS_WITH},
        {"JSON_EXTRACT", Opcode::SBLR3_JSON_EXTRACT},
        {"JSON_EXISTS", Opcode::SBLR3_FUNC_JSON_EXISTS},
        {"JSON_HAS_KEY", Opcode::SBLR3_FUNC_JSON_HAS_KEY},
        {"JSON_OBJECT", Opcode::SBLR3_JSON_OBJECT},
        {"JSON_ARRAY", Opcode::SBLR3_JSON_ARRAY},
        {"JSON_SET", Opcode::SBLR3_JSON_SET},
        {"JSON_INSERT", Opcode::SBLR3_JSON_INSERT},
        {"JSON_REMOVE", Opcode::SBLR3_JSON_REMOVE},
        {"TO_CHAR", Opcode::SBLR3_FUNC_TO_CHAR},
        {"TO_DATE", Opcode::SBLR3_FUNC_TO_DATE},
        {"TO_TIMESTAMP", Opcode::SBLR3_FUNC_TO_TIMESTAMP},
        {"LEAST", Opcode::SBLR3_FUNC_LEAST},
        {"GREATEST", Opcode::SBLR3_FUNC_GREATEST},
        {"COUNT", Opcode::SBLR3_AGG_COUNT},
        {"SUM", Opcode::SBLR3_AGG_SUM},
        {"AVG", Opcode::SBLR3_AGG_AVG},
        {"MIN", Opcode::SBLR3_AGG_MIN},
        {"MAX", Opcode::SBLR3_AGG_MAX},
        {"STDDEV", Opcode::SBLR3_AGG_STDDEV_SAMP},
        {"STDDEV_SAMP", Opcode::SBLR3_AGG_STDDEV_SAMP},
        {"STDDEV_POP", Opcode::SBLR3_AGG_STDDEV_POP},
        {"VARIANCE", Opcode::SBLR3_AGG_VAR_SAMP},
        {"VAR_SAMP", Opcode::SBLR3_AGG_VAR_SAMP},
        {"VAR_POP", Opcode::SBLR3_AGG_VAR_POP},
        {"CORR", Opcode::SBLR3_AGG_CORR},
        {"COVAR_POP", Opcode::SBLR3_AGG_COVAR_POP},
        {"REGR_SLOPE", Opcode::SBLR3_AGG_REGR_SLOPE},
        {"REGR_INTERCEPT", Opcode::SBLR3_AGG_REGR_INTERCEPT},
        {"REGR_COUNT", Opcode::SBLR3_AGG_REGR_COUNT},
        {"REGR_R2", Opcode::SBLR3_AGG_REGR_R2},
        {"REGR_AVGX", Opcode::SBLR3_AGG_REGR_AVGX},
        {"REGR_AVGY", Opcode::SBLR3_AGG_REGR_AVGY},
        {"REGR_SXX", Opcode::SBLR3_AGG_REGR_SXX},
        {"REGR_SYY", Opcode::SBLR3_AGG_REGR_SYY},
        {"REGR_SXY", Opcode::SBLR3_AGG_REGR_SXY},
        {"XMLAGG", Opcode::SBLR3_XMLAGG},
        {"ARRAY_AGG", Opcode::SBLR3_ARRAY_AGG},
        {"TO_TSVECTOR", Opcode::SBLR3_TO_TSVECTOR},
        {"PLAINTO_TSQUERY", Opcode::SBLR3_PLAINTO_TSQUERY},
        {"TO_TSQUERY", Opcode::SBLR3_TO_TSQUERY},
        {"TSMATCH", Opcode::SBLR3_TSMATCH},
        {"TS_RANK", Opcode::SBLR3_TS_RANK},
    };

    if (expr->is_window && expr->window) {
        if (name == "ROW_NUMBER") {
            inst.opcode = op(Opcode::SBLR3_WIN_ROW_NUMBER);
        } else if (name == "RANK") {
            inst.opcode = op(Opcode::SBLR3_WIN_RANK);
        } else if (name == "DENSE_RANK") {
            inst.opcode = op(Opcode::SBLR3_WIN_DENSE_RANK);
        } else if (name == "LAG") {
            inst.opcode = op(Opcode::SBLR3_WIN_LAG);
        } else if (name == "LEAD") {
            inst.opcode = op(Opcode::SBLR3_WIN_LEAD);
        } else if (name == "FIRST_VALUE") {
            inst.opcode = op(Opcode::SBLR3_WIN_FIRST_VALUE);
        } else if (name == "LAST_VALUE") {
            inst.opcode = op(Opcode::SBLR3_WIN_LAST_VALUE);
        } else if (name == "NTH_VALUE") {
            inst.opcode = op(Opcode::SBLR3_WIN_NTH_VALUE);
        } else {
            // Unsupported window function name in V3 path: keep deterministic opcode.
            inst.opcode = op(Opcode::SBLR3_WIN_ROW_NUMBER);
        }
    } else {
        auto it = kFuncMap.find(name);
        if (it != kFuncMap.end()) {
            inst.opcode = op(it->second);
            if (it->second == Opcode::SBLR3_FUNC_NOW && current_timestamp_semantics) {
                inst.flags |= kFuncNowCurrentTimestampFlag;
            }
        } else {
            inst.opcode = op(Opcode::SBLR3_EXPR_FUNCTION_CALL);
        }
    }

    if (inst.opcode == op(Opcode::SBLR3_JSON_EXTRACT) && expr->arguments.size() >= 2) {
        // Canonical JSON_EXTRACT opcode uses binary expression payload shape.
        Value::Object payload;
        payload["lhs"] = Value(makeInstr(emitExpression(expr->arguments[0])));
        payload["rhs"] = Value(makeInstr(emitExpression(expr->arguments[1])));
        inst.payload = Value(std::move(payload));
        return inst;
    }

    Value::Object payload;
    bool is_agg = false;
    const char* op_name = scratchbird::sblr::v3::opcodeName(inst.opcode);
    if (op_name) {
        std::string op_str(op_name);
        if (op_str.rfind("SBLR3_AGG_", 0) == 0 || op_str == "SBLR3_ARRAY_AGG" || op_str == "SBLR3_XMLAGG") {
            is_agg = true;
        }
    }
    if (is_agg) {
        payload["distinct"] = Value(expr->distinct);
        if (expr->filter) {
            payload["filter"] = Value(makeInstr(emitExpression(expr->filter)));
        }
        if (!expr->order_by.empty()) {
            payload["order_by"] = toOrderBy(expr->order_by);
        }
    }
    payload["args"] = toExprList(expr->arguments);
    if (expr->is_window && expr->window) {
        payload["window"] = toWindowSpec(expr->window);
    }
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitCast(parser::v3::CastExpr* expr) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_EXPR_CAST);
    inst.flags = 0;
    Value::Object payload;
    payload["value"] = Value(makeInstr(emitExpression(expr->expr)));
    payload["type"] = Value(buildTypeSpec(expr->target_type));
    if (expr->format.has_value() &&
        expr->format.value() != parser::v3::StringPool::INVALID_ID) {
        payload["format"] = toIdent(expr->format.value());
    }
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitCase(parser::v3::CaseExpr* expr) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_CASE_WHEN);
    inst.flags = 0;
    Value::Object payload;
    if (expr->operand) payload["base"] = Value(makeInstr(emitExpression(expr->operand)));
    payload["when_count"] = Value(uint64_t(expr->when_clauses.size()));
    if (!expr->when_clauses.empty()) {
        payload["when"] = Value(makeInstr(emitExpression(expr->when_clauses.front().when_expr)));
        payload["then"] = Value(makeInstr(emitExpression(expr->when_clauses.front().then_expr)));
    }
    if (expr->else_expr) payload["else"] = Value(makeInstr(emitExpression(expr->else_expr)));
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitIn(parser::v3::InExpr* expr) {
    Instruction inst;
    if (expr->has_subquery) {
        inst.opcode = op(expr->negated ? Opcode::SBLR3_SUBQUERY_NOT_IN : Opcode::SBLR3_SUBQUERY_IN);
    } else {
        inst.opcode = op(Opcode::SBLR3_IN_LIST);
    }
    inst.flags = 0;
    Value::Object payload;
    payload["value"] = Value(makeInstr(emitExpression(expr->expr)));
    Value::List list;
    if (expr->has_subquery && expr->subquery) {
        Instruction sub;
        sub.opcode = op(Opcode::SBLR3_SUBQUERY_SCALAR);
        sub.flags = 0;
        sub.payload = Value(Value::Object{{"query", Value(makeInstr(emitSelect(expr->subquery)))}}); 
        list.push_back(Value(makeInstr(sub)));
    } else {
        for (auto* v : expr->values) list.push_back(Value(makeInstr(emitExpression(v))));
    }
    payload["list"] = Value(std::move(list));
    payload["negated"] = Value(expr->negated);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitBetween(parser::v3::BetweenExpr* expr) {
    // Emit as (expr >= low AND expr <= high)
    auto left = emitExpression(expr->expr);
    Instruction ge;
    ge.opcode = op(Opcode::SBLR3_EXPR_GE);
    ge.flags = 0;
    ge.payload = Value(Value::Object{
        {"lhs", Value(makeInstr(left))},
        {"rhs", Value(makeInstr(emitExpression(expr->low)))}});

    Instruction le;
    le.opcode = op(Opcode::SBLR3_EXPR_LE);
    le.flags = 0;
    le.payload = Value(Value::Object{
        {"lhs", Value(makeInstr(left))},
        {"rhs", Value(makeInstr(emitExpression(expr->high)))}});

    Instruction and_inst;
    and_inst.opcode = op(Opcode::SBLR3_EXPR_AND);
    and_inst.flags = 0;
    and_inst.payload = Value(Value::Object{
        {"lhs", Value(makeInstr(ge))},
        {"rhs", Value(makeInstr(le))}});

    if (expr->negated) {
        Instruction not_inst;
        not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
        not_inst.flags = 0;
        not_inst.payload = Value(Value::Object{{"value", Value(makeInstr(and_inst))}});
        return not_inst;
    }
    return and_inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitLike(parser::v3::LikeExpr* expr) {
    Instruction inst;
    inst.flags = 0;

    if (expr->match_kind == parser::v3::LikeMatchKind::CONTAINING ||
        expr->match_kind == parser::v3::LikeMatchKind::STARTING) {
        inst.opcode = op(expr->match_kind == parser::v3::LikeMatchKind::CONTAINING
                             ? Opcode::SBLR3_PRED_CONTAINING
                             : Opcode::SBLR3_PRED_STARTING_WITH);
        Value::Object payload;
        payload["lhs"] = Value(makeInstr(emitExpression(expr->expr)));
        payload["rhs"] = Value(makeInstr(emitExpression(expr->pattern)));
        inst.payload = Value(std::move(payload));
        if (expr->negated) {
            Instruction not_inst;
            not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
            not_inst.flags = 0;
            not_inst.payload = Value(Value::Object{{"value", Value(makeInstr(inst))}});
            return not_inst;
        }
        return inst;
    }

    if (expr->match_kind == parser::v3::LikeMatchKind::SIMILAR) {
        inst.opcode = op(expr->case_insensitive ? Opcode::SBLR3_REGEX_MATCH_CI : Opcode::SBLR3_REGEX_MATCH);
        Value::Object payload;
        payload["lhs"] = Value(makeInstr(emitExpression(expr->expr)));
        payload["rhs"] = Value(makeInstr(emitExpression(expr->pattern)));
        inst.payload = Value(std::move(payload));
        if (expr->negated) {
            Instruction not_inst;
            not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
            not_inst.flags = 0;
            not_inst.payload = Value(Value::Object{{"value", Value(makeInstr(inst))}});
            return not_inst;
        }
        return inst;
    }

    inst.opcode = op(expr->case_insensitive ? Opcode::SBLR3_EXPR_ILIKE : Opcode::SBLR3_EXPR_LIKE);
    Value::Object payload;
    payload["value"] = Value(makeInstr(emitExpression(expr->expr)));
    payload["pattern"] = Value(makeInstr(emitExpression(expr->pattern)));
    if (expr->escape) payload["escape"] = Value(makeInstr(emitExpression(expr->escape)));
    payload["negated"] = Value(expr->negated);
    inst.payload = Value(std::move(payload));
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitExists(parser::v3::ExistsExpr* expr) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_SUBQUERY_EXISTS);
    inst.flags = 0;
    Value::Object payload;
    if (expr->subquery) payload["query"] = Value(makeInstr(emitSelect(expr->subquery)));
    inst.payload = Value(std::move(payload));
    if (expr->negated) {
        Instruction not_inst;
        not_inst.opcode = op(Opcode::SBLR3_EXPR_NOT);
        not_inst.flags = 0;
        not_inst.payload = Value(Value::Object{{"value", Value(makeInstr(inst))}});
        return not_inst;
    }
    return inst;
}

scratchbird::sblr::v3::Instruction V3Emitter::emitSubquery(parser::v3::SubqueryExpr* expr) {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_SUBQUERY_SCALAR);
    inst.flags = 0;
    Value::Object payload;
    if (expr->subquery) payload["query"] = Value(makeInstr(emitSelect(expr->subquery)));
    inst.payload = Value(std::move(payload));
    return inst;
}

Value V3Emitter::toIdent(parser::v3::StringPool::StringId id) {
    if (id == parser::v3::StringPool::INVALID_ID) return Value(std::string());
    return Value(std::string(pool_.get(id)));
}

Value V3Emitter::toSchemaPath(const parser::v3::SchemaPath& path) {
    Value::List parts;
    for (auto id : path.components) {
        parts.push_back(toIdent(id));
    }
    return Value(std::move(parts));
}

Value V3Emitter::toExprList(const std::vector<parser::v3::Expression*>& exprs) {
    Value::List list;
    list.reserve(exprs.size());
    for (auto* expr : exprs) {
        list.push_back(Value(makeInstr(emitExpression(expr))));
    }
    return Value(std::move(list));
}

Value V3Emitter::toSelectItems(const std::vector<parser::v3::SelectItem*>& items) {
    Value::List list;
    for (auto* item : items) {
        if (item->item_type == parser::v3::SelectItem::Type::EXPRESSION && item->expr) {
            list.push_back(Value(makeInstr(emitExpression(item->expr))));
        } else if (item->item_type == parser::v3::SelectItem::Type::STAR) {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_SELECT_STAR);
            inst.flags = 0;
            inst.payload = Value(Value::Bytes{});
            list.push_back(Value(makeInstr(inst)));
        } else if (item->item_type == parser::v3::SelectItem::Type::TABLE_STAR) {
            Instruction inst;
            inst.opcode = op(Opcode::SBLR3_SELECT_TABLE_STAR);
            inst.flags = 0;
            inst.payload = Value(Value::Bytes{});
            list.push_back(Value(makeInstr(inst)));
        }
    }
    return Value(std::move(list));
}

Value V3Emitter::toOrderBy(const std::vector<parser::v3::OrderByItem*>& items) {
    Value::List list;
    for (auto* item : items) {
        Value::Object o;
        o["expr"] = Value(makeInstr(emitExpression(item->expr)));
        o["order"] = Value(uint64_t(mapSortOrder(item->ascending)));
        o["nulls"] = Value(uint64_t(mapNullsOrder(item)));
        list.push_back(Value(std::move(o)));
    }
    return Value(std::move(list));
}

Value V3Emitter::toWindowSpec(parser::v3::WindowSpec* spec) {
    Value::Object w;
    if (!spec) {
        w["partition_by"] = Value(Value::List{});
        w["order_by"] = Value(Value::List{});
        return Value(std::move(w));
    }

    w["partition_by"] = toExprList(spec->partition_by);
    w["order_by"] = toOrderBy(spec->order_by);

    if (spec->has_frame) {
        auto map_frame_type = [](parser::v3::FrameType type) -> uint64_t {
            switch (type) {
                case parser::v3::FrameType::ROWS:
                    return 0;
                case parser::v3::FrameType::RANGE:
                    return 1;
                case parser::v3::FrameType::GROUPS:
                    return 2;
            }
            return 0;
        };
        auto map_bound_kind = [](parser::v3::FrameBoundType bound) -> uint64_t {
            switch (bound) {
                case parser::v3::FrameBoundType::UNBOUNDED_PRECEDING:
                    return 0;
                case parser::v3::FrameBoundType::UNBOUNDED_FOLLOWING:
                    return 1;
                case parser::v3::FrameBoundType::CURRENT_ROW:
                    return 2;
                case parser::v3::FrameBoundType::VALUE_PRECEDING:
                    return 3;
                case parser::v3::FrameBoundType::VALUE_FOLLOWING:
                    return 4;
            }
            return 2;
        };
        auto map_frame_exclusion = [](parser::FrameExclusion exclusion) -> uint64_t {
            switch (exclusion) {
                case parser::FrameExclusion::NO_OTHERS:
                    return 0;
                case parser::FrameExclusion::CURRENT_ROW:
                    return 1;
                case parser::FrameExclusion::GROUP:
                    return 2;
                case parser::FrameExclusion::TIES:
                    return 3;
            }
            return 0;
        };

        Value::Object frame;
        frame["unit"] = Value(map_frame_type(spec->frame_type));

        Value::Object start;
        start["kind"] = Value(map_bound_kind(spec->frame_start));
        if (spec->frame_start_value) {
            start["offset"] = Value(makeInstr(emitExpression(spec->frame_start_value)));
        }
        frame["start"] = Value(std::move(start));

        Value::Object end;
        end["kind"] = Value(map_bound_kind(spec->frame_end));
        if (spec->frame_end_value) {
            end["offset"] = Value(makeInstr(emitExpression(spec->frame_end_value)));
        }
        frame["end"] = Value(std::move(end));

        frame["explicit_between"] = Value(spec->frame_end != parser::v3::FrameBoundType::CURRENT_ROW ||
                                          spec->frame_end_value != nullptr);
        frame["exclusion"] = Value(map_frame_exclusion(spec->frame_exclusion));
        w["frame"] = Value(std::move(frame));
    }

    return Value(std::move(w));
}

Value V3Emitter::toTableRef(parser::v3::TableRefNode* node) {
    Value::Object o;
    if (node->ref_type == parser::v3::TableRefNode::Type::TABLE) {
        o["table_path"] = toSchemaPath(node->table_path);
    } else {
        // Subquery/function references are encoded as empty table_path with alias only (placeholder).
        o["table_path"] = Value(Value::List{});
    }
    if (node->has_alias) o["alias"] = toIdent(node->alias);
    uint64_t table_flags = 0;
    if (node->lateral) table_flags |= 0x0001;
    if (node->with_ordinality) table_flags |= 0x0002;
    if (node->sample_method != parser::v3::TableSampleMethod::NONE) {
        table_flags |= 0x0004;
        auto map_sample_method = [](parser::v3::TableSampleMethod method) -> uint64_t {
            switch (method) {
                case parser::v3::TableSampleMethod::BERNOULLI:
                    return 1;
                case parser::v3::TableSampleMethod::SYSTEM:
                    return 2;
                case parser::v3::TableSampleMethod::NONE:
                default:
                    return 0;
            }
        };
        Value::Object sample;
        sample["method"] = Value(map_sample_method(node->sample_method));
        if (node->sample_percent) {
            sample["percent"] = Value(makeInstr(emitExpression(node->sample_percent)));
        }
        if (node->sample_repeatable_seed) {
            sample["repeatable_seed"] = Value(makeInstr(emitExpression(node->sample_repeatable_seed)));
        }
        o["table_sample"] = Value(std::move(sample));
    }
    o["table_flags"] = Value(table_flags);
    return Value(std::move(o));
}

Value V3Emitter::toTableRefFromPath(const parser::v3::SchemaPath& path, parser::v3::StringPool::StringId alias) {
    Value::Object o;
    o["table_path"] = toSchemaPath(path);
    if (alias != parser::v3::StringPool::INVALID_ID) o["alias"] = toIdent(alias);
    o["table_flags"] = Value(uint64_t(0));
    return Value(std::move(o));
}

Value V3Emitter::toJoins(const std::vector<parser::v3::JoinNode*>& joins) {
    Value::List list;
    for (auto* join : joins) {
        Value::Object j;
        j["type"] = Value(uint64_t(mapJoinType(join->join_type)));
        if (join->right) j["right"] = toTableRef(join->right);
        if (join->on_condition) j["condition"] = Value(makeInstr(emitExpression(join->on_condition)));
        Value::List using_cols;
        for (auto id : join->using_columns) using_cols.push_back(toIdent(id));
        j["using"] = Value(std::move(using_cols));
        list.push_back(Value(std::move(j)));
    }
    return Value(std::move(list));
}

Value V3Emitter::toStmtList(const std::vector<parser::v3::Statement*>& stmts) {
    Value::List list;
    for (auto* stmt : stmts) {
        if (!stmt) continue;
        list.push_back(Value(makeInstr(emitStatement(stmt))));
    }
    return Value(std::move(list));
}

Value V3Emitter::emitColumnDef(parser::v3::ColumnDef* col) {
    Value::Object payload;
    payload["name"] = toIdent(col->name);
    payload["type"] = Value(buildTypeSpec(col->type));
    if (col->type.is_array || col->type.array_size.has_value()) {
        payload["array_size"] = Value(uint64_t(col->type.array_size.value_or(0)));
    }

    uint16_t flags = 0;
    Expression* default_expr = nullptr;
    Expression* generated_expr = nullptr;
    Value identity;
    Value::List checks;
    parser::v3::StringPool::StringId collation = parser::v3::StringPool::INVALID_ID;

    for (const auto& c : col->constraints) {
        switch (c.type) {
            case parser::v3::ConstraintType::NOT_NULL:
                flags |= 0x0001;
                break;
            case parser::v3::ConstraintType::NULL_ALLOWED:
                flags |= 0x0002;
                break;
            case parser::v3::ConstraintType::DEFAULT:
                default_expr = c.default_expr;
                break;
            case parser::v3::ConstraintType::GENERATED:
                generated_expr = c.generated_expr;
                if (c.generated_expr) {
                    if (c.generated_stored || c.generated_always) {
                        flags |= 0x0004;
                    } else {
                        flags |= 0x0008;
                    }
                } else if (c.generated_always) {
                    flags |= 0x0004;
                }
                break;
            case parser::v3::ConstraintType::CHECK:
                if (c.check_expr) checks.push_back(Value(makeInstr(emitExpression(c.check_expr))));
                break;
            case parser::v3::ConstraintType::COLLATE:
                collation = c.collation;
                break;
            default:
                break;
        }
    }

    if (col->is_computed) {
        generated_expr = col->computed_expr;
        flags |= col->computed_stored ? 0x0004 : 0x0008;
    }

    payload["flags"] = Value(uint64_t(flags));
    if (default_expr) payload["default_expr"] = Value(makeInstr(emitExpression(default_expr)));
    if (generated_expr) payload["generated_expr"] = Value(makeInstr(emitExpression(generated_expr)));
    if (!identity.isNull()) payload["identity"] = identity;
    if (collation != parser::v3::StringPool::INVALID_ID) payload["collation"] = toIdent(collation);
    if (col->charset != parser::v3::StringPool::INVALID_ID) payload["charset"] = toIdent(col->charset);
    payload["check_count"] = Value(uint64_t(checks.size()));
    if (!checks.empty()) payload["check_expr"] = checks.front();
    return Value(std::move(payload));
}

Value V3Emitter::emitTableConstraint(parser::v3::TableConstraint* c) {
    Value::Object payload;
    uint8_t type = 4;
    if (c->type == parser::v3::TableConstraintType::PRIMARY_KEY) type = 1;
    if (c->type == parser::v3::TableConstraintType::UNIQUE) type = 2;
    if (c->type == parser::v3::TableConstraintType::FOREIGN_KEY) type = 3;
    if (c->type == parser::v3::TableConstraintType::EXCLUDE) type = 5;
    payload["type"] = Value(uint64_t(type));
    if (c->name != parser::v3::StringPool::INVALID_ID) payload["name"] = toIdent(c->name);
    Value::List cols;
    for (auto id : c->columns) cols.push_back(toIdent(id));
    payload["columns"] = Value(std::move(cols));
    if (c->type == parser::v3::TableConstraintType::FOREIGN_KEY) {
        payload["ref_table"] = toSchemaPath(c->ref_table);
        Value::List refcols;
        for (auto id : c->ref_columns) refcols.push_back(toIdent(id));
        payload["ref_columns"] = Value(std::move(refcols));
        payload["on_update"] = Value(uint64_t(static_cast<uint8_t>(c->on_update)));
        payload["on_delete"] = Value(uint64_t(static_cast<uint8_t>(c->on_delete)));
    }
    if (c->type == parser::v3::TableConstraintType::CHECK && c->check_expr) {
        payload["check_expr"] = Value(makeInstr(emitExpression(c->check_expr)));
    }
    if (c->type == parser::v3::TableConstraintType::EXCLUDE) {
        if (c->index_method != parser::v3::StringPool::INVALID_ID) {
            payload["index_method"] = toIdent(c->index_method);
        }
        Value::List ex_expr;
        for (auto* expr : c->exclude_expressions) {
            if (!expr) continue;
            ex_expr.push_back(Value(makeInstr(emitExpression(expr))));
        }
        payload["exclude_expr"] = Value(std::move(ex_expr));
        Value::List ex_ops;
        for (auto op_id : c->exclude_operators) {
            ex_ops.push_back(toIdent(op_id));
        }
        payload["exclude_ops"] = Value(std::move(ex_ops));
        if (c->exclude_where) {
            payload["exclude_where"] = Value(makeInstr(emitExpression(c->exclude_where)));
        }
    }
    if (c->deferrable) {
        payload["deferrable"] = Value(true);
    } else if (c->not_deferrable) {
        payload["deferrable"] = Value(false);
    }
    if (c->initially_deferred) {
        payload["initially_deferred"] = Value(true);
    } else if (c->initially_immediate) {
        payload["initially_deferred"] = Value(false);
    }
    return Value(std::move(payload));
}

Value V3Emitter::emitColumnRefValue(parser::v3::StringPool::StringId column_id) {
    Value::Object payload;
    payload["path"] = Value(Value::List{});
    payload["column"] = toIdent(column_id);
    return Value(std::move(payload));
}

Value V3Emitter::emitVarRefValue(parser::v3::StringPool::StringId name) {
    Value::Object payload;
    payload["name"] = toIdent(name);
    return Value(std::move(payload));
}

scratchbird::sblr::v3::Instruction V3Emitter::emitLiteralZero() {
    Instruction inst;
    inst.opcode = op(Opcode::SBLR3_LITERAL_INT64);
    inst.flags = 0;
    inst.payload = Value(Value::Object{{"value", Value(int64_t(0))}});
    return inst;
}

TypeSpec V3Emitter::buildTypeSpec(const parser::v3::TypeName& type) {
    std::string name;
    if (type.has_schema_path) {
        if (!type.schema_path.components.empty()) {
            name = std::string(pool_.get(type.schema_path.components.back()));
        }
    } else if (type.name != parser::v3::StringPool::INVALID_ID) {
        name = std::string(pool_.get(type.name));
    }
    std::string upper = toUpper(name);
    if (upper == "TIME" && type.with_time_zone) upper = "TIME_TZ";
    if (upper == "TIMESTAMP" && type.with_time_zone) upper = "TIMESTAMP_TZ";

    auto type_signature = [&]() -> std::string {
        std::string signature;
        if (type.has_schema_path) {
            signature = schemaPathToString(pool_, type.schema_path);
        } else {
            signature = name;
        }

        if (!type.type_arguments.empty()) {
            signature.push_back('(');
            for (size_t i = 0; i < type.type_arguments.size(); ++i) {
                if (i > 0) {
                    signature.append(", ");
                }
                signature.append(pool_.get(type.type_arguments[i]));
            }
            signature.push_back(')');
        } else if (type.precision.has_value()) {
            signature.push_back('(');
            signature.append(std::to_string(*type.precision));
            if (type.scale.has_value()) {
                signature.push_back(',');
                signature.append(std::to_string(*type.scale));
            }
            signature.push_back(')');
        }

        if (type.is_array) {
            signature.push_back('[');
            if (type.array_size.has_value()) {
                signature.append(std::to_string(*type.array_size));
            }
            signature.push_back(']');
        }
        return signature;
    };

    static const std::unordered_map<std::string, Opcode> kTypeMap = {
        {"INT", Opcode::SBLR3_TYPE_INTEGER},
        {"INTEGER", Opcode::SBLR3_TYPE_INTEGER},
        {"BIGINT", Opcode::SBLR3_TYPE_BIGINT},
        {"SMALLINT", Opcode::SBLR3_TYPE_INT16},
        {"TINYINT", Opcode::SBLR3_TYPE_INT8},
        {"INT128", Opcode::SBLR3_TYPE_INT128},
        {"INT2", Opcode::SBLR3_TYPE_INT16},
        {"INT4", Opcode::SBLR3_TYPE_INTEGER},
        {"INT8", Opcode::SBLR3_TYPE_INT8},
        {"UINT8", Opcode::SBLR3_TYPE_UINT8},
        {"UINT16", Opcode::SBLR3_TYPE_UINT16},
        {"UINT32", Opcode::SBLR3_TYPE_UINT32},
        {"UINT64", Opcode::SBLR3_TYPE_UINT64},
        {"UINT128", Opcode::SBLR3_TYPE_UINT128},
        {"DECIMAL", Opcode::SBLR3_TYPE_DECIMAL},
        {"NUMERIC", Opcode::SBLR3_TYPE_DECIMAL},
        {"BIGNUM", Opcode::SBLR3_TYPE_DECIMAL},
        {"REAL", Opcode::SBLR3_TYPE_FLOAT32},
        {"FLOAT", Opcode::SBLR3_TYPE_FLOAT32},
        {"DOUBLE", Opcode::SBLR3_TYPE_DOUBLE},
        {"DOUBLE PRECISION", Opcode::SBLR3_TYPE_DOUBLE},
        {"BOOLEAN", Opcode::SBLR3_TYPE_BOOLEAN},
        {"BOOL", Opcode::SBLR3_TYPE_BOOLEAN},
        {"CHAR", Opcode::SBLR3_TYPE_CHAR},
        {"CHARACTER", Opcode::SBLR3_TYPE_CHAR},
        {"VARCHAR", Opcode::SBLR3_TYPE_VARCHAR},
        {"TEXT", Opcode::SBLR3_TYPE_TEXT},
        {"DATE", Opcode::SBLR3_TYPE_DATE},
        {"TIME", Opcode::SBLR3_TYPE_TIME},
        {"TIMESTAMP", Opcode::SBLR3_TYPE_TIMESTAMP},
        {"TIME_TZ", Opcode::SBLR3_TYPE_TIME_TZ},
        {"TIMESTAMP_TZ", Opcode::SBLR3_TYPE_TIMESTAMP_TZ},
        {"UUID", Opcode::SBLR3_TYPE_UUID},
        {"JSON", Opcode::SBLR3_TYPE_JSON},
        {"JSONB", Opcode::SBLR3_TYPE_JSONB},
        {"JSONPATH", Opcode::SBLR3_TYPE_JSONPATH},
        {"BLOB", Opcode::SBLR3_TYPE_BLOB},
        {"BLOB_TEXT", Opcode::SBLR3_TYPE_BLOB_TEXT},
        {"BYTEA", Opcode::SBLR3_TYPE_BYTEA},
        {"VARBINARY", Opcode::SBLR3_TYPE_VARBINARY},
        {"BINARY", Opcode::SBLR3_TYPE_BINARY},
        {"XML", Opcode::SBLR3_TYPE_XML},
        {"MONEY", Opcode::SBLR3_TYPE_MONEY},
        {"INTERVAL", Opcode::SBLR3_TYPE_INTERVAL},
        {"INET", Opcode::SBLR3_TYPE_INET},
        {"CIDR", Opcode::SBLR3_TYPE_CIDR},
        {"MACADDR", Opcode::SBLR3_TYPE_MACADDR},
        {"MACADDR8", Opcode::SBLR3_TYPE_MACADDR8},
        {"BIT", Opcode::SBLR3_TYPE_BIT},
        {"YEAR", Opcode::SBLR3_TYPE_YEAR},
        {"DATETIME", Opcode::SBLR3_TYPE_DATETIME},
        {"MEDIUMINT", Opcode::SBLR3_TYPE_MEDIUMINT},
        {"TSVECTOR", Opcode::SBLR3_TYPE_TSVECTOR},
        {"TSQUERY", Opcode::SBLR3_TYPE_TSQUERY},
        {"VECTOR", Opcode::SBLR3_TYPE_VECTOR},
        {"GEOMETRY", Opcode::SBLR3_TYPE_GEOMETRY},
        {"POINT", Opcode::SBLR3_TYPE_POINT},
        {"LINESTRING", Opcode::SBLR3_TYPE_LINESTRING},
        {"POLYGON", Opcode::SBLR3_TYPE_POLYGON},
        {"MULTIPOINT", Opcode::SBLR3_TYPE_MULTIPOINT},
        {"MULTILINESTRING", Opcode::SBLR3_TYPE_MULTILINESTRING},
        {"MULTIPOLYGON", Opcode::SBLR3_TYPE_MULTIPOLYGON},
        {"GEOMETRYCOLLECTION", Opcode::SBLR3_TYPE_GEOMETRYCOLLECTION},
        {"ENUM", Opcode::SBLR3_TYPE_ENUM},
        {"SET", Opcode::SBLR3_TYPE_SET},
        {"ROW", Opcode::SBLR3_TYPE_ROW},
        {"COMPOSITE", Opcode::SBLR3_TYPE_COMPOSITE},
        {"DOMAIN", Opcode::SBLR3_TYPE_DOMAIN},
        {"VARIANT", Opcode::SBLR3_TYPE_VARIANT},
        {"DYNAMIC", Opcode::SBLR3_TYPE_VARIANT},
        {"ARRAY", Opcode::SBLR3_TYPE_ARRAY},
        {"QBIT", Opcode::SBLR3_TYPE_BIT},
        {"INT4RANGE", Opcode::SBLR3_TYPE_INT4RANGE},
        {"INT8RANGE", Opcode::SBLR3_TYPE_INT8RANGE},
        {"NUMRANGE", Opcode::SBLR3_TYPE_NUMRANGE},
        {"DATERANGE", Opcode::SBLR3_TYPE_DATERANGE},
        {"TSRANGE", Opcode::SBLR3_TYPE_TSRANGE},
        {"TSTZRANGE", Opcode::SBLR3_TYPE_TSTZRANGE},
    };

    TypeSpec spec;
    if (upper == "AGGREGATEFUNCTION" || upper == "SIMPLEAGGREGATEFUNCTION") {
        std::string signature = type_signature();
        spec.type_opcode = op(Opcode::SBLR3_TYPE_DOMAIN);
        if (!signature.empty()) {
            spec.type_payload.assign(signature.begin(), signature.end());
        }
    } else {
        auto it = kTypeMap.find(upper);
        if (it != kTypeMap.end()) {
            spec.type_opcode = op(it->second);
        } else {
            std::string domain_ref = type_signature();
            if (!domain_ref.empty()) {
                spec.type_opcode = op(Opcode::SBLR3_TYPE_DOMAIN);
                spec.type_payload.assign(domain_ref.begin(), domain_ref.end());
            } else {
                spec.type_opcode = op(Opcode::SBLR3_TYPE_UNKNOWN);
            }
        }

        if (spec.type_opcode == op(Opcode::SBLR3_TYPE_DECIMAL) && upper == "BIGNUM") {
            std::string signature = type_signature();
            if (!signature.empty()) {
                spec.type_payload.assign(signature.begin(), signature.end());
            }
        }
        if (spec.type_opcode == op(Opcode::SBLR3_TYPE_VARIANT) && upper == "DYNAMIC") {
            std::string signature = type_signature();
            if (!signature.empty()) {
                spec.type_payload.assign(signature.begin(), signature.end());
            }
        }
        if (spec.type_opcode == op(Opcode::SBLR3_TYPE_BIT) && upper == "QBIT") {
            std::string signature = type_signature();
            if (!signature.empty()) {
                spec.type_payload.assign(signature.begin(), signature.end());
            }
        }
        if (spec.type_opcode == op(Opcode::SBLR3_TYPE_DOMAIN) && type.has_schema_path) {
            std::string signature = type_signature();
            if (!signature.empty() && spec.type_payload.empty()) {
                spec.type_payload.assign(signature.begin(), signature.end());
            }
        }
        if (spec.type_opcode == op(Opcode::SBLR3_TYPE_UNKNOWN)) {
            std::string signature = type_signature();
            if (!signature.empty()) {
                spec.type_opcode = op(Opcode::SBLR3_TYPE_DOMAIN);
                spec.type_payload.assign(signature.begin(), signature.end());
            }
        }
    }

    return spec;
}

Value::Bytes V3Emitter::encodeInstructionBytes(const Instruction& inst) {
    Buffer out;
    DecodeError err;
    if (!scratchbird::sblr::v3::encodeInstructionWithSchema(inst, out, err)) {
        scratchbird::sblr::v3::encodeInstruction(inst, out);
    }
    return out;
}

}  // namespace scratchbird::parser::v3
