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

const Value::InstrPtr* payloadExprField(const Value::Object& payload, const char* key) {
    auto it = payload.find(key);
    if (it == payload.end()) {
        return nullptr;
    }
    return std::get_if<Value::InstrPtr>(&it->second.data);
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

}  // namespace

TEST(ParserV3NoSqlEmitterContractTest, MapsEquivalentNoSqlCommandsToAllBridgeOpcodes) {
    struct Case {
        const char* sql;
        Opcode opcode;
        uint64_t action;
    };

    const std::vector<Case> cases = {
        {"CQL KEYSPACE ks_main", Opcode::SBLR3_CQL_KEYSPACE, 1},
        {"CQL BATCH 'BEGIN BATCH ...'", Opcode::SBLR3_CQL_BATCH, 2},
        {"CQL TTL 'ttl(users)=600'", Opcode::SBLR3_CQL_TTL, 3},
        {"CQL WRITETIME 'writetime(users.email)'", Opcode::SBLR3_CQL_WRITETIME, 4},

        {"MONGO FIND '{\"active\":true}'", Opcode::SBLR3_MONGO_FIND, 5},
        {"MONGO AGGREGATE '[{\"$match\":{}}]'", Opcode::SBLR3_MONGO_AGGREGATE, 6},
        {"MONGO FIND AND MODIFY '{\"_id\":1}'", Opcode::SBLR3_MONGO_FIND_AND_MODIFY, 7},
        {"MONGO BULK WRITE '[{\"insertOne\":{}}]'", Opcode::SBLR3_MONGO_BULK_WRITE, 8},

        {"CYPHER MATCH 'MATCH (n) RETURN n'", Opcode::SBLR3_CYPHER_MATCH, 9},
        {"CYPHER MERGE 'MERGE (n:Person {id:1})'", Opcode::SBLR3_CYPHER_MERGE, 10},
        {"CYPHER UNWIND 'UNWIND [1,2] AS n RETURN n'", Opcode::SBLR3_CYPHER_UNWIND, 11},
        {"CYPHER CALL 'CALL db.labels()'", Opcode::SBLR3_CYPHER_CALL, 12},

        {"REDIS STRING 'SET k v'", Opcode::SBLR3_REDIS_STRING, 13},
        {"REDIS HASH 'HSET h k v'", Opcode::SBLR3_REDIS_HASH, 14},
        {"REDIS LIST 'LPUSH l v'", Opcode::SBLR3_REDIS_LIST, 15},
        {"REDIS SET 'SADD s v'", Opcode::SBLR3_REDIS_SET, 16},
        {"REDIS ZSET 'ZADD z 1 v'", Opcode::SBLR3_REDIS_ZSET, 17},
        {"REDIS STREAM 'XADD s * f v'", Opcode::SBLR3_REDIS_STREAM, 18},
        {"REDIS PUBSUB 'PUBLISH c msg'", Opcode::SBLR3_REDIS_PUBSUB, 19},

        {"MILVUS CREATE COLLECTION vecs_main", Opcode::SBLR3_MILVUS_CREATE_COLLECTION, 20},
        {"MILVUS DROP COLLECTION vecs_main", Opcode::SBLR3_MILVUS_DROP_COLLECTION, 21},
        {"MILVUS CREATE INDEX vecs_hnsw", Opcode::SBLR3_MILVUS_CREATE_INDEX, 22},
        {"MILVUS DROP INDEX vecs_hnsw", Opcode::SBLR3_MILVUS_DROP_INDEX, 23},
        {"MILVUS INSERT '[{\"id\":1}]'", Opcode::SBLR3_MILVUS_INSERT, 24},
        {"MILVUS DELETE 'id in [1,2]'", Opcode::SBLR3_MILVUS_DELETE, 25},
        {"MILVUS SEARCH 'vector=[0.1,0.2]'", Opcode::SBLR3_MILVUS_SEARCH, 26},
        {"MILVUS QUERY 'id >= 10'", Opcode::SBLR3_MILVUS_QUERY, 27},
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
        {"VACUUM", Opcode::SBLR3_ADMIN_VACUUM_ALIAS, 31},

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
        {"CLUSTER SHOW STATE", Opcode::SBLR3_CLUSTER_SHOW_STATE, 45},
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
    }
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
