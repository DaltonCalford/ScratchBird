#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/sblr/v3_payloads.h"

using scratchbird::parser::v3::Parser;
using scratchbird::parser::v3::V3Emitter;
using scratchbird::sblr::v3::Container;
using scratchbird::sblr::v3::DecodeError;
using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::Value;

namespace {

struct EmittedRoot {
    uint16_t opcode = 0;
    Value payload;
};

bool emitRootFromSql(const std::string& sql, EmittedRoot& out, std::string& err) {
    Parser parser(sql);
    auto parse = parser.parseStatement();
    if (!parse.success() || parse.statement() == nullptr) {
        std::ostringstream oss;
        oss << "parse failure";
        for (const auto& e : parse.errors()) {
            oss << " | " << e.message;
        }
        err = oss.str();
        return false;
    }

    V3Emitter emitter(parser.stringPool());
    Container container;
    if (!emitter.emitStatementToContainer(parse.statement(), container, err)) {
        return false;
    }

    if (container.bytecode_stream.empty()) {
        err = "empty bytecode stream";
        return false;
    }

    size_t offset = 0;
    DecodeError decode_err;
    Instruction version_inst;
    if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
            container.bytecode_stream.data(),
            container.bytecode_stream.size(),
            offset,
            version_inst,
            decode_err)) {
        err = decode_err.message;
        return false;
    }
    if (version_inst.opcode != static_cast<uint16_t>(Opcode::SBLR3_VERSION)) {
        err = "missing version header opcode";
        return false;
    }

    Instruction root_inst;
    if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
            container.bytecode_stream.data(),
            container.bytecode_stream.size(),
            offset,
            root_inst,
            decode_err)) {
        err = decode_err.message;
        return false;
    }

    out.opcode = root_inst.opcode;
    out.payload = root_inst.payload;
    return true;
}

const Value::Object* payloadObject(const EmittedRoot& emitted) {
    return std::get_if<Value::Object>(&emitted.payload.data);
}

const uint64_t* payloadU64(const Value::Object& payload, const char* key) {
    auto it = payload.find(key);
    if (it == payload.end()) {
        return nullptr;
    }
    return std::get_if<uint64_t>(&it->second.data);
}

const Value::List* payloadListField(const Value::Object& payload, const char* key) {
    auto it = payload.find(key);
    if (it == payload.end()) {
        return nullptr;
    }
    return std::get_if<Value::List>(&it->second.data);
}

const std::string* payloadStringField(const Value::Object& payload, const char* key) {
    auto it = payload.find(key);
    if (it == payload.end()) {
        return nullptr;
    }
    return std::get_if<std::string>(&it->second.data);
}

const Value::InstrPtr* payloadExprField(const Value::Object& payload, const char* key) {
    auto it = payload.find(key);
    if (it == payload.end()) {
        return nullptr;
    }
    return std::get_if<Value::InstrPtr>(&it->second.data);
}

const std::string* payloadLiteralStringExprField(const Value::Object& payload, const char* key) {
    const auto* expr = payloadExprField(payload, key);
    if (!expr || !expr->get() ||
        expr->get()->opcode != static_cast<uint16_t>(Opcode::SBLR3_LITERAL_STRING)) {
        return nullptr;
    }
    const auto* expr_payload = std::get_if<Value::Object>(&expr->get()->payload.data);
    if (!expr_payload) {
        return nullptr;
    }
    return payloadStringField(*expr_payload, "value");
}

const Instruction* firstSelectItemExpr(const EmittedRoot& emitted) {
    const auto* payload = payloadObject(emitted);
    if (!payload) {
        return nullptr;
    }
    auto it = payload->find("select_items");
    if (it == payload->end()) {
        return nullptr;
    }
    const auto* list = std::get_if<Value::List>(&it->second.data);
    if (!list || list->empty()) {
        return nullptr;
    }
    const auto* ptr = std::get_if<Value::InstrPtr>(&list->front().data);
    if (!ptr || !ptr->get()) {
        return nullptr;
    }
    return ptr->get();
}

const Value::List* selectAliases(const EmittedRoot& emitted) {
    const auto* payload = payloadObject(emitted);
    if (!payload) {
        return nullptr;
    }
    return payloadListField(*payload, "select_aliases");
}

}  // namespace

TEST(ParserV3NoSqlEmitterContractTest, MapsCanonicalRedisKvAndStreamCommandsToBridgeOpcodes) {
    struct Case {
        const char* sql;
        Opcode opcode;
        uint64_t action;
    };

    const std::vector<Case> cases = {
        {"REDIS STRING 'SET k v'", Opcode::SBLR3_REDIS_STRING, 13},
        {"REDIS HASH 'HSET h k v'", Opcode::SBLR3_REDIS_HASH, 14},
        {"REDIS LIST 'LPUSH l v'", Opcode::SBLR3_REDIS_LIST, 15},
        {"REDIS SET 'SADD s v'", Opcode::SBLR3_REDIS_SET, 16},
        {"REDIS ZSET 'ZADD z 1 v'", Opcode::SBLR3_REDIS_ZSET, 17},
        {"REDIS STREAM 'XADD s * f v'", Opcode::SBLR3_REDIS_STREAM, 18},
    };

    for (const auto& c : cases) {
        EmittedRoot emitted;
        std::string err;
        ASSERT_TRUE(emitRootFromSql(c.sql, emitted, err)) << c.sql << " | " << err;
        EXPECT_EQ(static_cast<uint16_t>(c.opcode), emitted.opcode) << c.sql;

        const auto* payload = payloadObject(emitted);
        ASSERT_NE(nullptr, payload) << c.sql;

        const uint64_t* action = payloadU64(*payload, "action");
        ASSERT_NE(nullptr, action) << c.sql;
        EXPECT_EQ(c.action, *action) << c.sql;

        const auto* query_expr = payloadExprField(*payload, "query_expr");
        ASSERT_NE(nullptr, query_expr) << c.sql;
        ASSERT_NE(nullptr, query_expr->get()) << c.sql;

        const auto* options = payloadListField(*payload, "options");
        ASSERT_NE(nullptr, options) << c.sql;
        EXPECT_TRUE(options->empty()) << c.sql;
    }
}

TEST(ParserV3NoSqlEmitterContractTest, PreservesSelectItemAliasesInSelectPayload) {
    EmittedRoot emitted;
    std::string err;
    ASSERT_TRUE(emitRootFromSql(
        "SELECT id AS order_id, name FROM users",
        emitted,
        err)) << err;
    ASSERT_EQ(static_cast<uint16_t>(Opcode::SBLR3_SELECT), emitted.opcode);

    const auto* aliases = selectAliases(emitted);
    ASSERT_NE(aliases, nullptr);
    ASSERT_EQ(aliases->size(), 2u);

    const auto* first_alias = std::get_if<std::string>(&(*aliases)[0].data);
    ASSERT_NE(first_alias, nullptr);
    EXPECT_EQ(*first_alias, "order_id");

    const auto* second_alias = std::get_if<std::string>(&(*aliases)[1].data);
    ASSERT_NE(second_alias, nullptr);
    EXPECT_TRUE(second_alias->empty());
}

TEST(ParserV3NoSqlEmitterContractTest, RejectsRemovedEnginePrefixedAliasesBeforeEmission) {
    const std::vector<const char*> removed_aliases = {
        "CQL KEYSPACE ks_main",
        "MONGO FIND '{\"active\":true}'",
        "CYPHER MATCH 'MATCH (n) RETURN n'",
        "MILVUS QUERY 'id >= 10'",
        "REDIS PUBSUB 'PUBLISH c msg'",
    };

    for (const char* sql : removed_aliases) {
        EmittedRoot emitted;
        std::string err;
        EXPECT_FALSE(emitRootFromSql(sql, emitted, err)) << sql;
        EXPECT_NE(err.find("PRS_0505"), std::string::npos) << sql << " | " << err;
    }
}

TEST(ParserV3NoSqlEmitterContractTest, MapsAdminClusterAndServiceCommandsToBridgeOpcodes) {
    struct Case {
        const char* sql;
        Opcode opcode;
        uint64_t action;
    };

    const std::vector<Case> cases = {
        {"BACKUP DATABASE '/tmp/scratchbird.sbk'", Opcode::SBLR3_ADMIN_BACKUP, 28},
        {"RESTORE DATABASE '/tmp/scratchbird.sbk'", Opcode::SBLR3_ADMIN_RESTORE, 29},
        {"VALIDATE DATABASE", Opcode::SBLR3_ADMIN_VALIDATE, 30},

        {"CREATE CLUSTER WORKLOAD CLASS wl_oltp 'MAX=64'",
         Opcode::SBLR3_CLUSTER_WORKLOAD_CLASS, 32},
        {"ALTER CLUSTER WORKLOAD CLASS wl_oltp 'MAX=32'",
         Opcode::SBLR3_CLUSTER_WORKLOAD_CLASS, 33},
        {"DROP CLUSTER WORKLOAD CLASS wl_oltp",
         Opcode::SBLR3_CLUSTER_WORKLOAD_CLASS, 34},

        {"CREATE CLUSTER WORKLOAD ROUTE rt_hot 'CLASS=wl_oltp'",
         Opcode::SBLR3_CLUSTER_WORKLOAD_ROUTE, 35},
        {"ALTER CLUSTER WORKLOAD ROUTE rt_hot 'CLASS=wl_olap'",
         Opcode::SBLR3_CLUSTER_WORKLOAD_ROUTE, 36},
        {"DROP CLUSTER WORKLOAD ROUTE rt_hot",
         Opcode::SBLR3_CLUSTER_WORKLOAD_ROUTE, 37},

        {"CREATE CLUSTER ADMISSION POLICY ap_ingress 'QPS<=1000'",
         Opcode::SBLR3_CLUSTER_ADMISSION_POLICY, 38},
        {"ALTER CLUSTER ADMISSION POLICY ap_ingress 'QPS<=800'",
         Opcode::SBLR3_CLUSTER_ADMISSION_POLICY, 39},
        {"DROP CLUSTER ADMISSION POLICY ap_ingress",
         Opcode::SBLR3_CLUSTER_ADMISSION_POLICY, 40},

        {"CREATE CLUSTER ADMISSION BINDING ab_ingress 'CLASS=wl_oltp'",
         Opcode::SBLR3_CLUSTER_ADMISSION_BINDING, 41},
        {"ALTER CLUSTER ADMISSION BINDING ab_ingress 'CLASS=wl_olap'",
         Opcode::SBLR3_CLUSTER_ADMISSION_BINDING, 42},
        {"DROP CLUSTER ADMISSION BINDING ab_ingress",
         Opcode::SBLR3_CLUSTER_ADMISSION_BINDING, 43},

        {"CLUSTER SET STATE 'READ_WRITE'", Opcode::SBLR3_CLUSTER_SET_STATE, 44},
        {"SHOW CLUSTER ROUTING PLAN", Opcode::SBLR3_CLUSTER_SHOW_ROUTING_PLAN, 46},
        {"SHOW CLUSTER ADMISSION STATUS", Opcode::SBLR3_CLUSTER_SHOW_ADMISSION_STATUS, 47},

        {"SERVICE CHANNEL BACKUP 'channel=primary'",
         Opcode::SBLR3_SERVICE_CHANNEL_BACKUP, 48},
        {"SERVICE CHANNEL EVENTS 'since=0'",
         Opcode::SBLR3_SERVICE_CHANNEL_EVENTS, 49},
        {"SERVICE CHANNEL PROGRESS 'job=1'",
         Opcode::SBLR3_SERVICE_CHANNEL_PROGRESS, 50},

        {"CREATE CUBE sales_cube AS SELECT region, sum(amount) FROM sales GROUP BY region",
         Opcode::SBLR3_CUBE_DDL, 51},
        {"ALTER CUBE sales_cube REBUILD INCREMENTAL",
         Opcode::SBLR3_CUBE_DDL, 52},
        {"DROP CUBE IF EXISTS sales_cube",
         Opcode::SBLR3_CUBE_DDL, 53},
        {"REFRESH CUBE sales_cube FULL",
         Opcode::SBLR3_CUBE_REFRESH, 54},
        {"SHOW CUBE STATS sales_cube",
         Opcode::SBLR3_CUBE_SHOW_STATS, 55},
    };

    for (const auto& c : cases) {
        EmittedRoot emitted;
        std::string err;
        ASSERT_TRUE(emitRootFromSql(c.sql, emitted, err)) << c.sql << " | " << err;
        EXPECT_EQ(static_cast<uint16_t>(c.opcode), emitted.opcode) << c.sql;

        const auto* payload = payloadObject(emitted);
        ASSERT_NE(nullptr, payload) << c.sql;

        const uint64_t* action = payloadU64(*payload, "action");
        ASSERT_NE(nullptr, action) << c.sql;
        EXPECT_EQ(c.action, *action) << c.sql;

        const auto* options = payloadListField(*payload, "options");
        ASSERT_NE(nullptr, options) << c.sql;
        EXPECT_TRUE(options->empty()) << c.sql;

        if (c.action >= 32 && c.action <= 43) {
            const auto* object_name = payloadStringField(*payload, "object_name");
            ASSERT_NE(nullptr, object_name) << c.sql;
            EXPECT_FALSE(object_name->empty()) << c.sql;

            const bool is_drop = c.action == 34 || c.action == 37 ||
                                 c.action == 40 || c.action == 43;
            if (!is_drop) {
                const auto* value_expr = payloadExprField(*payload, "value");
                ASSERT_NE(nullptr, value_expr) << c.sql;
                ASSERT_NE(nullptr, value_expr->get()) << c.sql;
                const auto* literal = payloadLiteralStringExprField(*payload, "value");
                ASSERT_NE(nullptr, literal) << c.sql;
                EXPECT_FALSE(literal->empty()) << c.sql;
            }
        }

        if (c.action == 46) {
            const auto* object_name = payloadStringField(*payload, "object_name");
            ASSERT_NE(nullptr, object_name) << c.sql;
            EXPECT_EQ("routing_plan", *object_name) << c.sql;
        }

        if (c.action == 47) {
            const auto* object_name = payloadStringField(*payload, "object_name");
            ASSERT_NE(nullptr, object_name) << c.sql;
            EXPECT_EQ("admission_status", *object_name) << c.sql;
        }
    }
}

TEST(ParserV3NoSqlEmitterContractTest, EmitsIfExistsPayloadForGovernanceDrops) {
    EmittedRoot emitted;
    std::string err;
    ASSERT_TRUE(emitRootFromSql("DROP CLUSTER IF EXISTS ADMISSION POLICY ap_ingress", emitted, err))
        << err;
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_CLUSTER_ADMISSION_POLICY), emitted.opcode);

    const auto* payload = payloadObject(emitted);
    ASSERT_NE(nullptr, payload);

    const uint64_t* action = payloadU64(*payload, "action");
    ASSERT_NE(nullptr, action);
    EXPECT_EQ(40u, *action);

    const auto* object_name = payloadStringField(*payload, "object_name");
    ASSERT_NE(nullptr, object_name);
    EXPECT_EQ("ap_ingress", *object_name);

    const auto* literal = payloadLiteralStringExprField(*payload, "value");
    ASSERT_NE(nullptr, literal);
    EXPECT_EQ("IF_EXISTS=1", *literal);
}

TEST(ParserV3NoSqlEmitterContractTest, EmitsDedicatedWindowFunctionOpcodes) {
    struct Case {
        const char* sql;
        Opcode opcode;
    };

    const std::vector<Case> cases = {
        {"SELECT LAG(v) OVER (ORDER BY v) FROM t", Opcode::SBLR3_WIN_LAG},
        {"SELECT LEAD(v, 2, 0) OVER (ORDER BY v) FROM t", Opcode::SBLR3_WIN_LEAD},
        {"SELECT FIRST_VALUE(v) OVER (ORDER BY v) FROM t", Opcode::SBLR3_WIN_FIRST_VALUE},
        {"SELECT LAST_VALUE(v) OVER (ORDER BY v) FROM t", Opcode::SBLR3_WIN_LAST_VALUE},
        {"SELECT NTH_VALUE(v, 2) OVER (ORDER BY v) FROM t", Opcode::SBLR3_WIN_NTH_VALUE},
    };

    for (const auto& c : cases) {
        EmittedRoot emitted;
        std::string err;
        ASSERT_TRUE(emitRootFromSql(c.sql, emitted, err)) << c.sql << " | " << err;
        EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_SELECT), emitted.opcode) << c.sql;

        const auto* expr = firstSelectItemExpr(emitted);
        ASSERT_NE(nullptr, expr) << c.sql;
        EXPECT_EQ(static_cast<uint16_t>(c.opcode), expr->opcode) << c.sql;
    }
}

TEST(ParserV3NoSqlEmitterContractTest, PreservesJsonExistsOperatorSpecificityFlags) {
    struct Case {
        const char* sql;
        uint16_t expected_flags;
    };

    const std::vector<Case> cases = {
        {"SELECT '{\"a\":1}' ? 'a'", 0x0000},
        {"SELECT '{\"a\":1}' ?| ARRAY['a','z']", 0x0001},
        {"SELECT '{\"a\":1}' ?& ARRAY['a','z']", 0x0002},
    };

    for (const auto& c : cases) {
        EmittedRoot emitted;
        std::string err;
        ASSERT_TRUE(emitRootFromSql(c.sql, emitted, err)) << c.sql << " | " << err;
        EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_SELECT), emitted.opcode) << c.sql;

        const auto* expr = firstSelectItemExpr(emitted);
        ASSERT_NE(nullptr, expr) << c.sql;
        EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_FUNC_JSON_EXISTS), expr->opcode) << c.sql;
        EXPECT_EQ(c.expected_flags, expr->flags) << c.sql;
    }
}
