#include <gtest/gtest.h>

#include "scratchbird/sblr/v3_canonical_feature_map.generated.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_opcode_registry.h"

using namespace scratchbird::sblr::v3;

TEST(SBLRV3CanonicalFeatureMap, LoadsAuthoritativeRows) {
    EXPECT_GE(canonicalFeatureRowCount(), static_cast<std::size_t>(150));
    EXPECT_TRUE(isCanonicalFeatureOpcodeSymbol("OP_STMT_DML_SELECT"));
    EXPECT_TRUE(isCanonicalFeatureOpcodeSymbol("OP_STMT_DDL_CREATE_TABLE"));
    EXPECT_FALSE(isCanonicalFeatureOpcodeSymbol("OP_STMT_UNKNOWN_NOPE"));
}

TEST(SBLRV3OpcodeIdentity, MapsKnownStatementOpcodes) {
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SELECT"), "OP_STMT_DML_SELECT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_INSERT"), "OP_STMT_DML_INSERT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_UPDATE"), "OP_STMT_DML_UPDATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DELETE"), "OP_STMT_DML_DELETE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_TABLE"), "OP_STMT_DDL_CREATE_TABLE");

    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SELECT"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_TABLE"));
}

TEST(SBLRV3OpcodeIdentity, MapsExpandedStatementFamilies) {
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_DATABASE"),
              "OP_STMT_DDL_CREATE_DATABASE_NATIVE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_DATABASE_EMULATED"),
              "OP_STMT_DDL_CREATE_DATABASE_EMULATED");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_SCHEMA"),
              "OP_STMT_DDL_CREATE_SCHEMA");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_SEQUENCE"),
              "OP_STMT_DDL_CREATE_SEQUENCE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_TYPE"),
              "OP_STMT_DDL_CREATE_TYPE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_DOMAIN_RECORD"),
              "OP_STMT_DDL_CREATE_DOMAIN_RECORD");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_DOMAIN_ENUM"),
              "OP_STMT_DDL_CREATE_DOMAIN_ENUM");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_DOMAIN_SET"),
              "OP_STMT_DDL_CREATE_DOMAIN_SET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_DOMAIN_RANGE"),
              "OP_STMT_DDL_CREATE_DOMAIN_RANGE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_TABLE_AS"),
              "OP_STMT_DDL_CREATE_MATERIALIZED_VIEW");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_DB_TRIGGER"),
              "OP_STMT_DDL_CREATE_EVENT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REFRESH_MATERIALIZED_VIEW"),
              "OP_STMT_DDL_REFRESH_MATERIALIZED_VIEW");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_COPY"), "OP_STMT_DML_COPY_BULK");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_PREPARE_STMT"),
              "OP_STMT_PREPARE_STATEMENT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_EXECUTE_PREPARED"),
              "OP_STMT_EXECUTE_PREPARED");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DEALLOCATE_PREPARED"),
              "OP_STMT_DEALLOCATE_PREPARED");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_FOREIGN_DATA_WRAPPER"),
              "OP_STMT_FDW_CREATE_WRAPPER");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALTER_FOREIGN_DATA_WRAPPER"),
              "OP_STMT_FDW_ALTER_WRAPPER");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DROP_FOREIGN_DATA_WRAPPER"),
              "OP_STMT_FDW_DROP_WRAPPER");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_FOREIGN_SERVER"),
              "OP_STMT_FDW_CREATE_SERVER");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALTER_FOREIGN_SERVER"),
              "OP_STMT_FDW_ALTER_SERVER");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_USER_MAPPING"),
              "OP_STMT_FDW_USER_MAPPING");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALTER_USER_MAPPING"),
              "OP_STMT_FDW_ALTER_USER_MAPPING");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_FOREIGN_TABLE"),
              "OP_STMT_FDW_FOREIGN_TABLE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALTER_FOREIGN_TABLE"),
              "OP_STMT_FDW_ALTER_FOREIGN_TABLE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_IMPORT_FOREIGN_SCHEMA"),
              "OP_STMT_FDW_IMPORT_SCHEMA");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DROP_FOREIGN_SERVER"),
              "OP_STMT_FDW_DROP_SERVER");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DROP_FOREIGN_TABLE"),
              "OP_STMT_FDW_DROP_FOREIGN_TABLE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DROP_USER_MAPPING"),
              "OP_STMT_FDW_DROP_USER_MAPPING");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ANALYZE_REMOTE_SERVER"),
              "OP_STMT_REMOTE_ANALYZE_METADATA");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REFRESH_REMOTE_METADATA"),
              "OP_STMT_REMOTE_REFRESH_METADATA");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_REMOTE_CAPABILITIES"),
              "OP_STMT_REMOTE_SHOW_CAPABILITIES");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_REMOTE_OBJECTS"),
              "OP_STMT_REMOTE_SHOW_OBJECTS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_REMOTE_COLUMNS"),
              "OP_STMT_REMOTE_SHOW_COLUMNS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_REMOTE_STATISTICS"),
              "OP_STMT_REMOTE_SHOW_STATISTICS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_EXECUTE_REMOTE"),
              "OP_STMT_REMOTE_EXECUTE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_PREPARE_REMOTE"),
              "OP_STMT_REMOTE_PREPARE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_EXECUTE_REMOTE_PREPARED"),
              "OP_STMT_REMOTE_EXECUTE_PREPARED");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DEALLOCATE_REMOTE_PREPARED"),
              "OP_STMT_REMOTE_DEALLOCATE_PREPARED");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_BEGIN_REMOTE_TRANSACTION"),
              "OP_STMT_REMOTE_BEGIN_TXN");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_COMMIT_REMOTE_TRANSACTION"),
              "OP_STMT_REMOTE_COMMIT_TXN");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ROLLBACK_REMOTE_TRANSACTION"),
              "OP_STMT_REMOTE_ROLLBACK_TXN");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_REMOTE_SESSION_STATE"),
              "OP_STMT_REMOTE_SHOW_SESSION_STATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ANALYZE"), "OP_STMT_ADMIN_ANALYZE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SWEEP"), "OP_STMT_ADMIN_SWEEP");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ADMIN_BACKUP"), "OP_STMT_ADMIN_BACKUP");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ADMIN_RESTORE"), "OP_STMT_ADMIN_RESTORE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ADMIN_VALIDATE"), "OP_STMT_ADMIN_VALIDATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ADMIN_VACUUM_ALIAS"),
              "OP_COMPAT_ADMIN_VACUUM_ALIAS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SET_VARIABLE"), "OP_STMT_SESSION_SET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_VARIABLE"), "OP_STMT_SESSION_SHOW");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_SYSTEM"), "OP_STMT_SHOW_SYSTEM_OBJECTS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_ALL"), "OP_STMT_CONFIG_SHOW");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALTER_SYSTEM"), "OP_STMT_CONFIG_SET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SESSION_RESET"), "OP_STMT_SESSION_RESET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CONFIG_RESET"), "OP_STMT_CONFIG_RESET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CONFIG_HISTORY"), "OP_STMT_CONFIG_HISTORY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CONFIG_RELOAD"), "OP_STMT_CONFIG_RELOAD");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CONFIG_RESOURCE_BUNDLES_SHOW"),
              "OP_STMT_CONFIG_RESOURCE_BUNDLES_SHOW");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CONFIG_RESOURCE_BUNDLE_VALIDATE"),
              "OP_STMT_CONFIG_RESOURCE_BUNDLE_VALIDATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CONFIG_RESOURCE_BUNDLE_ACTIVATE"),
              "OP_STMT_CONFIG_RESOURCE_BUNDLE_ACTIVATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_RETURNING"), "OP_STMT_DML_RETURNING");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_EXECUTE_JOB"), "OP_STMT_JOB_SUBMIT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_JOB"), "OP_STMT_JOB_SCHEDULE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CANCEL_JOB_RUN"), "OP_STMT_JOB_CANCEL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALTER_JOB"), "OP_STMT_JOB_RETRY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_JOB"), "OP_STMT_JOB_SHOW");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_INDEX"), "OP_STMT_IDX_SHOW_OPTIONS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHOW_INDEXES"), "OP_STMT_IDX_SHOW_USAGE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_INDEX_REINDEX"), "OP_STMT_IDX_REBUILD");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_INDEX_VACUUM"), "OP_STMT_IDX_REBALANCE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_INDEX_STATS"), "OP_STMT_IDX_ANALYZE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_INDEX_SEARCH"), "OP_STMT_IDX_LIGHT_SCAN");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_INDEX_SCAN"), "OP_STMT_IDX_DIAGNOSTIC_SCAN");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_INDEX_TYPE"), "OP_STMT_IDX_SHOW_STORAGE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_IDX_SET_OPTIONS"),
              "OP_STMT_IDX_SET_OPTIONS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_IDX_RESET_OPTIONS"),
              "OP_STMT_IDX_RESET_OPTIONS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_IDX_RELOCATE"), "OP_STMT_IDX_RELOCATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_IDX_DEFAULTS_SET"),
              "OP_STMT_IDX_DEFAULTS_SET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_IDX_DEFAULTS_RESET"),
              "OP_STMT_IDX_DEFAULTS_RESET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_IDX_SHOW_HEALTH"), "OP_STMT_IDX_SHOW_HEALTH");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_IDX_SHOW_CONTENTION"),
              "OP_STMT_IDX_SHOW_CONTENTION");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TEXTSEARCH_CREATE_DICTIONARY"),
              "OP_STMT_TEXTSEARCH_CREATE_DICTIONARY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TEXTSEARCH_ALTER_DICTIONARY"),
              "OP_STMT_TEXTSEARCH_ALTER_DICTIONARY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TEXTSEARCH_DROP_DICTIONARY"),
              "OP_STMT_TEXTSEARCH_DROP_DICTIONARY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TEXTSEARCH_CREATE_CONFIGURATION"),
              "OP_STMT_TEXTSEARCH_CREATE_CONFIGURATION");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TEXTSEARCH_ALTER_CONFIGURATION"),
              "OP_STMT_TEXTSEARCH_ALTER_CONFIGURATION");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TEXTSEARCH_DROP_CONFIGURATION"),
              "OP_STMT_TEXTSEARCH_DROP_CONFIGURATION");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TEXTSEARCH_LOAD_DICTIONARY_DATA"),
              "OP_STMT_TEXTSEARCH_LOAD_DICTIONARY_DATA");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CQL_KEYSPACE"), "OP_STMT_CQL_KEYSPACE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CQL_BATCH"), "OP_STMT_CQL_BATCH");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CQL_TTL"), "OP_STMT_CQL_TTL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CQL_WRITETIME"), "OP_STMT_CQL_WRITETIME");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MONGO_FIND"), "OP_STMT_MONGO_FIND");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MONGO_AGGREGATE"),
              "OP_STMT_MONGO_AGGREGATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MONGO_FIND_AND_MODIFY"),
              "OP_STMT_MONGO_FIND_AND_MODIFY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MONGO_BULK_WRITE"),
              "OP_STMT_MONGO_BULK_WRITE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CYPHER_MATCH"), "OP_STMT_CYPHER_MATCH");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CYPHER_MERGE"), "OP_STMT_CYPHER_MERGE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CYPHER_UNWIND"), "OP_STMT_CYPHER_UNWIND");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CYPHER_CALL"), "OP_STMT_CYPHER_CALL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REDIS_STRING"), "OP_STMT_REDIS_STRING");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REDIS_HASH"), "OP_STMT_REDIS_HASH");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REDIS_LIST"), "OP_STMT_REDIS_LIST");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REDIS_SET"), "OP_STMT_REDIS_SET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REDIS_ZSET"), "OP_STMT_REDIS_ZSET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REDIS_STREAM"), "OP_STMT_REDIS_STREAM");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_REDIS_PUBSUB"), "OP_STMT_REDIS_PUBSUB");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MILVUS_CREATE_COLLECTION"),
              "OP_STMT_MILVUS_CREATE_COLLECTION");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MILVUS_DROP_COLLECTION"),
              "OP_STMT_MILVUS_DROP_COLLECTION");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MILVUS_CREATE_INDEX"),
              "OP_STMT_MILVUS_CREATE_INDEX");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MILVUS_DROP_INDEX"),
              "OP_STMT_MILVUS_DROP_INDEX");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MILVUS_INSERT"), "OP_STMT_MILVUS_INSERT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MILVUS_DELETE"), "OP_STMT_MILVUS_DELETE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MILVUS_SEARCH"), "OP_STMT_MILVUS_SEARCH");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_MILVUS_QUERY"), "OP_STMT_MILVUS_QUERY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CLUSTER_WORKLOAD_CLASS"),
              "OP_STMT_CLUSTER_WORKLOAD_CLASS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CLUSTER_WORKLOAD_ROUTE"),
              "OP_STMT_CLUSTER_WORKLOAD_ROUTE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CLUSTER_ADMISSION_POLICY"),
              "OP_STMT_CLUSTER_ADMISSION_POLICY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CLUSTER_ADMISSION_BINDING"),
              "OP_STMT_CLUSTER_ADMISSION_BINDING");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CLUSTER_SET_STATE"),
              "OP_STMT_CLUSTER_SET_STATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CLUSTER_SHOW_STATE"),
              "OP_STMT_CLUSTER_SHOW_STATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CLUSTER_SHOW_ROUTING_PLAN"),
              "OP_STMT_CLUSTER_SHOW_ROUTING_PLAN");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CLUSTER_SHOW_ADMISSION_STATUS"),
              "OP_STMT_CLUSTER_SHOW_ADMISSION_STATUS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALERT_RULE_DDL"), "OP_STMT_ALERT_RULE_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALERT_TARGET_DDL"),
              "OP_STMT_ALERT_TARGET_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALERT_ROUTE_DDL"),
              "OP_STMT_ALERT_ROUTE_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALERT_SILENCE_DDL"),
              "OP_STMT_ALERT_SILENCE_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALERT_ACK"), "OP_STMT_ALERT_ACK");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_ALERT_SHOW"), "OP_STMT_ALERT_SHOW");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_HEALING_POLICY_DDL"),
              "OP_STMT_HEALING_POLICY_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_HEALING_ACTION_DDL"),
              "OP_STMT_HEALING_ACTION_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_HEALING_RUN"), "OP_STMT_HEALING_RUN");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_HEALING_SHOW_RUNS"),
              "OP_STMT_HEALING_SHOW_RUNS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_JOB_TYPE_DDL"), "OP_STMT_JOB_TYPE_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_JOB_TYPE_PARAM_SET"),
              "OP_STMT_JOB_TYPE_PARAM_SET");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHARD_POLICY_DDL"),
              "OP_STMT_SHARD_POLICY_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHARD_DDL"), "OP_STMT_SHARD_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHARD_REPLICA_DDL"),
              "OP_STMT_SHARD_REPLICA_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHARD_MIGRATE"), "OP_STMT_SHARD_MIGRATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SHARD_SHOW"), "OP_STMT_SHARD_SHOW");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CUBE_DDL"), "OP_STMT_CUBE_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CUBE_REFRESH"), "OP_STMT_CUBE_REFRESH");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CUBE_SHOW_STATS"), "OP_STMT_CUBE_SHOW_STATS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SECURITY_ENCRYPTION_PROFILE"),
              "OP_STMT_SECURITY_ENCRYPTION_PROFILE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SECURITY_ENCRYPTION_KEY"),
              "OP_STMT_SECURITY_ENCRYPTION_KEY");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SECURITY_KEY_SHARD_SUBMIT"),
              "OP_STMT_SECURITY_KEY_SHARD_SUBMIT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SECURITY_UNLOCK_DATABASE"),
              "OP_STMT_SECURITY_UNLOCK_DATABASE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SECURITY_CERT_DDL"),
              "OP_STMT_SECURITY_CERT_DDL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SECURITY_PRIVATE_KEY_ROTATE"),
              "OP_STMT_SECURITY_PRIVATE_KEY_ROTATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SECURITY_SHOW_STATUS"),
              "OP_STMT_SECURITY_SHOW_STATUS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SERVICE_CHANNEL_BACKUP"),
              "OP_STMT_SERVICE_CHANNEL_BACKUP");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SERVICE_CHANNEL_EVENTS"),
              "OP_STMT_SERVICE_CHANNEL_EVENTS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SERVICE_CHANNEL_PROGRESS"),
              "OP_STMT_SERVICE_CHANNEL_PROGRESS");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_PSQL_POST_EVENT"), "OP_STMT_NOTIFY_PUBLISH");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CONNECT"), "OP_STMT_NOTIFY_SUBSCRIBE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DISCONNECT"), "OP_STMT_NOTIFY_UNSUBSCRIBE");

    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_DATABASE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_DATABASE_EMULATED"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_SCHEMA"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_DOMAIN_RECORD"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_DOMAIN_ENUM"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_DOMAIN_SET"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_DOMAIN_RANGE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_TABLE_AS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_DB_TRIGGER"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_COPY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_PREPARE_STMT"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_FOREIGN_DATA_WRAPPER"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALTER_FOREIGN_DATA_WRAPPER"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_DROP_FOREIGN_DATA_WRAPPER"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALTER_FOREIGN_SERVER"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALTER_USER_MAPPING"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALTER_FOREIGN_TABLE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_IMPORT_FOREIGN_SCHEMA"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_DROP_FOREIGN_SERVER"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_DROP_FOREIGN_TABLE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_DROP_USER_MAPPING"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ANALYZE_REMOTE_SERVER"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_REFRESH_REMOTE_METADATA"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_REMOTE_CAPABILITIES"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_REMOTE_OBJECTS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_REMOTE_COLUMNS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_REMOTE_STATISTICS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_EXECUTE_REMOTE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_PREPARE_REMOTE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_EXECUTE_REMOTE_PREPARED"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_DEALLOCATE_REMOTE_PREPARED"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_BEGIN_REMOTE_TRANSACTION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_COMMIT_REMOTE_TRANSACTION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ROLLBACK_REMOTE_TRANSACTION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_REMOTE_SESSION_STATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ANALYZE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ADMIN_BACKUP"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ADMIN_RESTORE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ADMIN_VALIDATE"));
    EXPECT_FALSE(opcodeMapsToCanonicalFeatureName("SBLR3_ADMIN_VACUUM_ALIAS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SET_VARIABLE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_VARIABLE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_SYSTEM"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SESSION_RESET"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CONFIG_RESET"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CONFIG_HISTORY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CONFIG_RELOAD"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CONFIG_RESOURCE_BUNDLES_SHOW"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CONFIG_RESOURCE_BUNDLE_VALIDATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CONFIG_RESOURCE_BUNDLE_ACTIVATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_RETURNING"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_EXECUTE_JOB"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_JOB"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CANCEL_JOB_RUN"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALTER_JOB"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_JOB"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_INDEX"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHOW_INDEXES"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_INDEX_REINDEX"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_INDEX_VACUUM"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_INDEX_STATS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_INDEX_SEARCH"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_INDEX_SCAN"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_INDEX_TYPE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_IDX_SET_OPTIONS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_IDX_RESET_OPTIONS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_IDX_RELOCATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_IDX_DEFAULTS_SET"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_IDX_DEFAULTS_RESET"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_IDX_SHOW_HEALTH"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_IDX_SHOW_CONTENTION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_TEXTSEARCH_CREATE_DICTIONARY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_TEXTSEARCH_ALTER_DICTIONARY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_TEXTSEARCH_DROP_DICTIONARY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_TEXTSEARCH_CREATE_CONFIGURATION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_TEXTSEARCH_ALTER_CONFIGURATION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_TEXTSEARCH_DROP_CONFIGURATION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_TEXTSEARCH_LOAD_DICTIONARY_DATA"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CQL_KEYSPACE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CQL_BATCH"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CQL_TTL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CQL_WRITETIME"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MONGO_FIND"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MONGO_AGGREGATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MONGO_FIND_AND_MODIFY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MONGO_BULK_WRITE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CYPHER_MATCH"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CYPHER_MERGE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CYPHER_UNWIND"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CYPHER_CALL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_REDIS_STRING"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_REDIS_HASH"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_REDIS_LIST"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_REDIS_SET"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_REDIS_ZSET"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_REDIS_STREAM"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_REDIS_PUBSUB"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MILVUS_CREATE_COLLECTION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MILVUS_DROP_COLLECTION"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MILVUS_CREATE_INDEX"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MILVUS_DROP_INDEX"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MILVUS_INSERT"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MILVUS_DELETE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MILVUS_SEARCH"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_MILVUS_QUERY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CLUSTER_WORKLOAD_CLASS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CLUSTER_WORKLOAD_ROUTE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CLUSTER_ADMISSION_POLICY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CLUSTER_ADMISSION_BINDING"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CLUSTER_SET_STATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CLUSTER_SHOW_STATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CLUSTER_SHOW_ROUTING_PLAN"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CLUSTER_SHOW_ADMISSION_STATUS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALERT_RULE_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALERT_TARGET_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALERT_ROUTE_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALERT_SILENCE_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALERT_ACK"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_ALERT_SHOW"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_HEALING_POLICY_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_HEALING_ACTION_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_HEALING_RUN"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_HEALING_SHOW_RUNS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_JOB_TYPE_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_JOB_TYPE_PARAM_SET"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHARD_POLICY_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHARD_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHARD_REPLICA_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHARD_MIGRATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SHARD_SHOW"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CUBE_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CUBE_REFRESH"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CUBE_SHOW_STATS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SECURITY_ENCRYPTION_PROFILE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SECURITY_ENCRYPTION_KEY"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SECURITY_KEY_SHARD_SUBMIT"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SECURITY_UNLOCK_DATABASE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SECURITY_CERT_DDL"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SECURITY_PRIVATE_KEY_ROTATE"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SECURITY_SHOW_STATUS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SERVICE_CHANNEL_BACKUP"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SERVICE_CHANNEL_EVENTS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SERVICE_CHANNEL_PROGRESS"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_PSQL_POST_EVENT"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CONNECT"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_DISCONNECT"));
}

TEST(SBLRV3OpcodeIdentity, MapsExpressionAndTypeFamilies) {
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_EXPR_SUBTRACT"), "OP_EXPR_SUB");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_EXPR_MULTIPLY"), "OP_EXPR_MUL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TYPE_UINT128"), "OP_TYPE_UINT128");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_LITERAL_UUID"), "OP_VAL_UUID");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_PSQL_FOR_SELECT"), "OP_FLOW_PSQL_FOR_SELECT");

    EXPECT_FALSE(opcodeMapsToCanonicalFeatureName("SBLR3_EXPR_SUBTRACT"));
}

TEST(SBLRV3OpcodeIdentity, MapsFromNumericOpcode) {
    const uint16_t select_opcode = static_cast<uint16_t>(Opcode::SBLR3_SELECT);
    const uint16_t version_opcode = static_cast<uint16_t>(Opcode::SBLR3_VERSION);

    EXPECT_EQ(canonicalOpcodeSymbolForOpcode(select_opcode), "OP_STMT_DML_SELECT");
    EXPECT_EQ(canonicalOpcodeSymbolForOpcode(version_opcode), "OP_MOD_BEGIN");
    EXPECT_TRUE(opcodeMapsToCanonicalFeature(select_opcode));
    EXPECT_FALSE(opcodeMapsToCanonicalFeature(version_opcode));
}
