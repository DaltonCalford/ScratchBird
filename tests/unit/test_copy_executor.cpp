/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * COPY executor tests (CSV/DELIMITER/NULL/HEADER)
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "unit/test_user_helpers.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>

using namespace scratchbird::sblr;
using namespace scratchbird::core;

namespace {
class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const std::string& value)
        : name_(name)
    {
        if (const char* existing = std::getenv(name_))
        {
            had_original_ = true;
            original_value_ = existing;
        }
        setenv(name_, value.c_str(), 1);
    }

    ~ScopedEnvVar()
    {
        if (had_original_)
        {
            setenv(name_, original_value_.c_str(), 1);
        }
        else
        {
            unsetenv(name_);
        }
    }

private:
    const char* name_;
    bool had_original_ = false;
    std::string original_value_;
};

void stripLeadingLegacyDebugSpans(std::vector<uint8_t>& bytecode)
{
    constexpr size_t kVersionPrefixSize = 2;
    constexpr size_t kDebugSpanPayloadSize = 1 + 2 + 4 + 4;
    size_t pos = kVersionPrefixSize;
    while (pos + kDebugSpanPayloadSize <= bytecode.size() &&
           bytecode[pos] == static_cast<uint8_t>(scratchbird::sblr::Opcode::EXTENDED_OPCODE))
    {
        const uint16_t ext_opcode =
            static_cast<uint16_t>(bytecode[pos + 1]) |
            (static_cast<uint16_t>(bytecode[pos + 2]) << 8);
        if (ext_opcode != static_cast<uint16_t>(scratchbird::sblr::ExtendedOpcode::EXT_DEBUG_SPAN))
        {
            break;
        }
        bytecode.erase(bytecode.begin() + static_cast<std::ptrdiff_t>(pos),
                       bytecode.begin() + static_cast<std::ptrdiff_t>(pos + kDebugSpanPayloadSize));
    }
}
} // namespace

static std::string makeUniquePath(const std::string& prefix, const std::string& suffix) {
    std::ostringstream oss;
    oss << "/tmp/" << prefix << "_"
        << std::this_thread::get_id() << "_"
        << std::chrono::steady_clock::now().time_since_epoch().count()
        << suffix;
    return oss.str();
}

class ScopedConfigOverride {
public:
    ScopedConfigOverride(const std::string& section,
                         const std::string& key,
                         const std::string& value)
        : section_(section), key_(key)
    {
        auto resolved = Config::getInstance().getResolvedValue(section_, key_);
        if (resolved.has_value())
        {
            had_previous_value_ = true;
            previous_value_ = resolved->value;
            previous_source_ = resolved->source;
        }
        Config::getInstance().set(section_, key_, value);
    }

    ~ScopedConfigOverride()
    {
        if (had_previous_value_ &&
            previous_source_ == Config::ValueSource::DURABLE_OVERRIDE)
        {
            Config::getInstance().set(section_, key_, previous_value_);
            return;
        }

        Config::getInstance().unsetDurableOverride(section_, key_);
    }

private:
    std::string section_;
    std::string key_;
    bool had_previous_value_ = false;
    std::string previous_value_;
    Config::ValueSource previous_source_ = Config::ValueSource::NONE;
};

class CopyExecutorTest : public ::testing::Test {
protected:
    struct HeapScanSummary {
        size_t row_count = 0;
        std::vector<std::string> tuple_headers;
    };

    void SetUp() override {
        db_path_ = makeUniquePath("test_copy_executor", ".sbdb");
        std::filesystem::remove(db_path_);

        ErrorContext ctx;
        auto status = Database::create(db_path_, 16384, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create test database";

        status = db_.open(db_path_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to open test database";

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        CatalogManager::SchemaInfo public_schema_info;
        status = catalog_->getSchema("public", public_schema_info, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to resolve public schema";
        public_schema_id_ = public_schema_info.schema_id;

        EnsureUser(catalog_, "test_user");

        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        executor_ = std::make_unique<Executor>(&db_);

        status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(status, Status::OK) << "Failed to create connection";
        connection_ctx_->setCurrentSchemaId(public_schema_id_);
        auto system_user_id = catalog_->getSystemUserId(&ctx);
        connection_ctx_->setCurrentUser(system_user_id, true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());
    }

    void TearDown() override {
        std::vector<std::string> tablespace_paths;
        if (catalog_ != nullptr)
        {
            ErrorContext ctx;
            std::vector<TablespaceInfo> tablespaces;
            if (catalog_->listTablespaces(tablespaces, &ctx) == Status::OK)
            {
                for (const auto& ts : tablespaces)
                {
                    if (ts.tablespace_id == PRIMARY_TABLESPACE_ID)
                    {
                        continue;
                    }
                    for (const auto& path : ts.file_paths)
                    {
                        tablespace_paths.push_back(path);
                    }
                }
            }
        }
        compiler_.reset();
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        for (const auto& path : tablespace_paths)
        {
            std::filesystem::remove(path);
        }
        std::filesystem::remove(db_path_);
        std::filesystem::remove(db_path_ + "-lock");
    }

    ExecutionResult compileAndExecute(const std::string& sql) {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success()) {
            std::string errors;
            for (const auto& err : compile_result.errors()) {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    ExecutionResult compileAndExecuteLegacyPg(const std::string& sql) {
        scratchbird::parser::postgresql::Parser parser(sql, &db_, "public");
        auto parse_result = parser.parseStatement();
        if (!parse_result.success()) {
            std::string errors;
            for (const auto& err : parse_result.errors()) {
                errors += err.message + "\n";
            }
            return ExecutionResult("Legacy PostgreSQL parse failed: " + errors);
        }
        auto legacy_bytecode = parse_result.bytecode();
        stripLeadingLegacyDebugSpans(legacy_bytecode);
        ScopedEnvVar scoped_legacy_execute("SCRATCHBIRD_ENABLE_LEGACY_EXECUTE", "1");
        return executor_->execute(legacy_bytecode);
    }

    HeapScanSummary scanHeapRows(const ID& table_id, bool ignore_visibility) {
        HeapScanSummary summary;
        ErrorContext ctx;
        auto scan = ignore_visibility ? db_.storage_engine()->createScanAll(table_id, &ctx)
                                      : db_.storage_engine()->createScan(table_id, &ctx);
        EXPECT_NE(scan, nullptr);
        if (!scan)
        {
            return summary;
        }

        Tuple tuple{};
        while (scan->next(&tuple, &ctx) == Status::OK)
        {
            summary.row_count++;
            if (tuple.data_size >= sizeof(TupleHeader))
            {
                const auto* header = reinterpret_cast<const TupleHeader*>(tuple.data);
                std::ostringstream oss;
                oss << "tid=" << getTablespaceID(tuple.tid.gpid) << ":" << getPageNumber(tuple.tid.gpid)
                    << ":" << tuple.tid.slot << " xmin=" << header->xmin << " xmax=" << header->xmax
                    << " ctid=" << getTablespaceID(header->ctid_gpid) << ":"
                    << getPageNumber(header->ctid_gpid) << ":" << header->ctid_slot;
                summary.tuple_headers.push_back(oss.str());
            }
        }
        return summary;
    }

    std::string formatHeapScanSummary(const HeapScanSummary& summary) {
        std::ostringstream oss;
        oss << "row_count=" << summary.row_count;
        for (const auto& header : summary.tuple_headers)
        {
            oss << "\n  " << header;
        }
        return oss.str();
    }

    std::string db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID public_schema_id_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(CopyExecutorTest, CopyCsvWithHeaderAndNulls) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_src (id INT, name VARCHAR(10), note VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO copy_src VALUES (1, 'alpha', NULL), "
        "(2, NULL, 'beta'), (3, 'a,b', 'c\"d')").success());

    std::string path = makeUniquePath("copy_out", ".csv");
    std::string sql = "COPY copy_src TO '" + path +
                      "' WITH (FORMAT csv, DELIMITER ',', NULL 'NULL', HEADER true)";
    auto copy_out = compileAndExecute(sql);
    ASSERT_TRUE(copy_out.success()) << copy_out.error();

    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());

    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0], "id,name,note");
    EXPECT_EQ(lines[1], "1,alpha,NULL");
    EXPECT_EQ(lines[2], "2,NULL,beta");
    EXPECT_EQ(lines[3], "3,\"a,b\",\"c\"\"d\"");

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyCsvFromWithHeaderAndDelimiter) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_dst (id INT, name VARCHAR(10), note VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    std::string path = makeUniquePath("copy_in", ".csv");
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "id|name|note\n";
        out << "1|alpha|NULL\n";
        out << "2|NULL|beta\n";
        out << "3|\"a|b\"|\"c\"\"d\"\n";
    }

    std::string sql = "COPY copy_dst FROM '" + path +
                      "' WITH (FORMAT csv, DELIMITER '|', NULL 'NULL', HEADER true)";
    auto copy_in = compileAndExecute(sql);
    ASSERT_TRUE(copy_in.success()) << copy_in.error();

    auto select = compileAndExecute("SELECT id, name, note FROM copy_dst ORDER BY id");
    ASSERT_TRUE(select.success()) << select.error();
    ASSERT_TRUE(select.hasResultSet());

    auto* results = select.resultSet();
    ASSERT_EQ(results->rowCount(), 3u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "1");
    EXPECT_EQ(results->getValue(0, 1).toString(), "alpha");
    EXPECT_TRUE(results->getValue(0, 2).isNull());

    EXPECT_EQ(results->getValue(1, 0).toString(), "2");
    EXPECT_TRUE(results->getValue(1, 1).isNull());
    EXPECT_EQ(results->getValue(1, 2).toString(), "beta");

    EXPECT_EQ(results->getValue(2, 0).toString(), "3");
    EXPECT_EQ(results->getValue(2, 1).toString(), "a|b");
    EXPECT_EQ(results->getValue(2, 2).toString(), "c\"d");

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, ResolveCopyBulkPlanUsesConfiguredRetailMicroBatchDefault) {
    ScopedConfigOverride scoped_batch_rows("sb.bulk", "micro_batch_target_rows", "257");

    const auto plan = Executor::resolveCopyBulkPlan(std::nullopt);
    EXPECT_EQ(plan.lane, Executor::CopyBulkLane::RETAIL_MICRO_BATCH);
    EXPECT_EQ(plan.batch_size, 257u);
    EXPECT_FALSE(plan.batch_size_explicit);
    EXPECT_STREQ(Executor::copyBulkLaneName(plan.lane), "RETAIL_MICRO_BATCH");
}

TEST_F(CopyExecutorTest, ResolveCopyBulkPlanHonorsExplicitBatchSizeOverride) {
    ScopedConfigOverride scoped_batch_rows("sb.bulk", "micro_batch_target_rows", "257");

    const auto plan = Executor::resolveCopyBulkPlan(uint32_t{9});
    EXPECT_EQ(plan.lane, Executor::CopyBulkLane::RETAIL_MICRO_BATCH);
    EXPECT_EQ(plan.batch_size, 9u);
    EXPECT_TRUE(plan.batch_size_explicit);
}

TEST_F(CopyExecutorTest, ClassifyCopyBulkLaneChoosesSortedExactBulkAtThreshold) {
    ScopedConfigOverride scoped_sorted_rows("sb.bulk", "sorted_exact_min_rows", "1000");

    const auto lane = Executor::classifyCopyBulkLane(1000u, true);
    EXPECT_EQ(lane, Executor::CopyBulkLane::SORTED_EXACT_BULK);
    EXPECT_STREQ(Executor::copyBulkLaneName(lane), "SORTED_EXACT_BULK");
}

TEST_F(CopyExecutorTest, ClassifyCopyBulkLaneChoosesShadowCutoverWhenRequested) {
    const auto lane = Executor::classifyCopyBulkLane(10u, false, true);
    EXPECT_EQ(lane, Executor::CopyBulkLane::SHADOW_LOAD_CUTOVER);
    EXPECT_STREQ(Executor::copyBulkLaneName(lane), "SHADOW_LOAD_CUTOVER");
}

TEST_F(CopyExecutorTest, CopyFromPublishesRetailBulkLoadCatalogState) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_bulk_plan_dst (id INT, name VARCHAR(10), note VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    std::string path = makeUniquePath("copy_bulk_plan_in", ".csv");
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "id|name|note\n";
        out << "1|alpha|NULL\n";
        out << "2|NULL|beta\n";
        out << "3|\"a|b\"|\"c\"\"d\"\n";
    }

    std::string sql = "COPY copy_bulk_plan_dst FROM '" + path +
                      "' WITH (FORMAT csv, DELIMITER '|', NULL 'NULL', HEADER true)";
    auto copy_in = compileAndExecute(sql);
    ASSERT_TRUE(copy_in.success()) << copy_in.error();

    ErrorContext ctx;
    CatalogManager::TableInfo table_info{};
    ASSERT_EQ(catalog_->getTable(public_schema_id_, "copy_bulk_plan_dst", table_info, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::BulkLoadPlanCatalogInfo> plans;
    ASSERT_EQ(catalog_->listBulkLoadPlanCatalogEntries(plans, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(plans.size(), 1u);

    const auto& plan = plans.front();
    EXPECT_EQ(plan.object_uuid, table_info.table_id);
    EXPECT_EQ(plan.ingest_lane, "RETAIL_MICRO_BATCH");
    EXPECT_EQ(plan.load_kind, "COPY_FROM");
    EXPECT_EQ(plan.source_format, "CSV");
    EXPECT_EQ(plan.phase_state, "COMPLETED");

    std::vector<CatalogManager::BulkLoadEventCatalogInfo> events;
    ASSERT_EQ(catalog_->listBulkLoadEventCatalogEntries(plan.bulk_load_plan_uuid, events, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(events.size(), 2u);
    EXPECT_TRUE(events[0].has_phase_from);
    EXPECT_EQ(events[0].phase_from, "PLANNED");
    EXPECT_EQ(events[0].phase_to, "RUNNING");
    EXPECT_EQ(events[0].event_state, "COMMITTED");
    EXPECT_EQ(events[0].event_code, "COPY_FROM_STARTED");
    EXPECT_TRUE(events[1].has_phase_from);
    EXPECT_EQ(events[1].phase_from, "RUNNING");
    EXPECT_EQ(events[1].phase_to, "COMPLETED");
    EXPECT_EQ(events[1].event_state, "COMMITTED");
    EXPECT_EQ(events[1].event_code, "COPY_FROM_COMPLETED");

    CatalogManager::BulkLoadProgressCatalogInfo progress{};
    ASSERT_EQ(catalog_->getBulkLoadProgressCatalogEntry(plan.bulk_load_plan_uuid, progress, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(progress.scanned_row_count, 3u);
    EXPECT_EQ(progress.written_row_count, 3u);
    EXPECT_EQ(progress.validated_row_count, 3u);
    EXPECT_EQ(progress.restart_disposition, "NONE");
    EXPECT_TRUE(progress.has_last_heartbeat_at);

    CatalogManager::BulkLoadCutoverGuardCatalogInfo guard{};
    ASSERT_EQ(catalog_->getBulkLoadCutoverGuardCatalogEntry(plan.bulk_load_plan_uuid, guard, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(guard.guard_state, "NOT_REQUIRED");

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyFromPublishesSortedExactBulkLoadCatalogState) {
    ScopedConfigOverride scoped_sorted_rows("sb.bulk", "sorted_exact_min_rows", "1000");

    auto create_table = compileAndExecute(
        "CREATE TABLE copy_sorted_bulk_plan_dst (id INT PRIMARY KEY, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    std::string path = makeUniquePath("copy_sorted_bulk_plan_in", ".csv");
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "id,name\n";
        for (int id = 1000; id >= 1; --id)
        {
            out << id << ",name" << id << "\n";
        }
    }

    std::string sql = "COPY copy_sorted_bulk_plan_dst FROM '" + path +
                      "' WITH (FORMAT csv, DELIMITER ',', HEADER true)";
    auto copy_in = compileAndExecute(sql);
    ASSERT_TRUE(copy_in.success()) << copy_in.error();

    ErrorContext ctx;
    std::vector<CatalogManager::BulkLoadPlanCatalogInfo> plans;
    ASSERT_EQ(catalog_->listBulkLoadPlanCatalogEntries(plans, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(plans.size(), 1u);
    EXPECT_EQ(plans.front().ingest_lane, "SORTED_EXACT_BULK");
    EXPECT_EQ(plans.front().phase_state, "COMPLETED");

    auto select = compileAndExecute(
        "SELECT id, name FROM copy_sorted_bulk_plan_dst ORDER BY id");
    ASSERT_TRUE(select.success()) << select.error();
    ASSERT_TRUE(select.hasResultSet());
    auto* results = select.resultSet();
    ASSERT_EQ(results->rowCount(), 1000u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "1");
    EXPECT_EQ(results->getValue(0, 1).toString(), "name1");
    EXPECT_EQ(results->getValue(999, 0).toString(), "1000");
    EXPECT_EQ(results->getValue(999, 1).toString(), "name1000");

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyFromShadowLoadRequestPublishesCommittedCutoverState) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_shadow_bulk_plan_dst (id INT PRIMARY KEY, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();
    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO copy_shadow_bulk_plan_dst VALUES (5, 'pre')").success());

    std::string path = makeUniquePath("copy_shadow_bulk_plan_in", ".csv");
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "id,name\n";
        out << "2,beta\n";
        out << "1,alpha\n";
    }

    std::string sql = "COPY copy_shadow_bulk_plan_dst FROM '" + path +
                      "' WITH (FORMAT csv, DELIMITER ',', HEADER true, SHADOW_LOAD true)";
    auto copy_in = compileAndExecute(sql);
    ASSERT_TRUE(copy_in.success()) << copy_in.error();

    ErrorContext ctx;
    std::vector<CatalogManager::BulkLoadPlanCatalogInfo> plans;
    ASSERT_EQ(catalog_->listBulkLoadPlanCatalogEntries(plans, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(plans.size(), 1u);
    EXPECT_EQ(plans.front().ingest_lane, "SHADOW_LOAD_CUTOVER");
    EXPECT_EQ(plans.front().phase_state, "CUTOVER_COMMITTED");

    std::vector<CatalogManager::BulkLoadEventCatalogInfo> events;
    ASSERT_EQ(catalog_->listBulkLoadEventCatalogEntries(plans.front().bulk_load_plan_uuid, events, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(events.size(), 3u);
    EXPECT_TRUE(events[0].has_phase_from);
    EXPECT_EQ(events[0].phase_from, "PLANNED");
    EXPECT_EQ(events[0].phase_to, "RUNNING");
    EXPECT_EQ(events[0].event_state, "COMMITTED");
    EXPECT_EQ(events[0].event_code, "COPY_FROM_STARTED");
    EXPECT_TRUE(events[1].has_phase_from);
    EXPECT_EQ(events[1].phase_from, "RUNNING");
    EXPECT_EQ(events[1].phase_to, "CUTOVER_PENDING");
    EXPECT_EQ(events[1].event_state, "COMMITTED");
    EXPECT_EQ(events[1].event_code, "SHADOW_LOAD_CUTOVER_READY");
    EXPECT_TRUE(events[2].has_phase_from);
    EXPECT_EQ(events[2].phase_from, "CUTOVER_PENDING");
    EXPECT_EQ(events[2].phase_to, "CUTOVER_COMMITTED");
    EXPECT_EQ(events[2].event_state, "COMMITTED");
    EXPECT_EQ(events[2].event_code, "SHADOW_LOAD_CUTOVER_COMMITTED");

    CatalogManager::BulkLoadCutoverGuardCatalogInfo guard{};
    ASSERT_EQ(catalog_->getBulkLoadCutoverGuardCatalogEntry(plans.front().bulk_load_plan_uuid, guard, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(guard.guard_state, "CUTOVER_COMMITTED");
    EXPECT_TRUE(guard.dependency_refresh_complete);

    CatalogManager::TableInfo table_info{};
    ASSERT_EQ(catalog_->getTable(public_schema_id_, "copy_shadow_bulk_plan_dst", table_info, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_NE(table_info.tablespace_id, PRIMARY_TABLESPACE_ID);

    const auto visible_heap = scanHeapRows(table_info.table_id, false);
    const auto raw_heap = scanHeapRows(table_info.table_id, true);
    EXPECT_EQ(raw_heap.row_count, 3u) << formatHeapScanSummary(raw_heap);
    EXPECT_EQ(visible_heap.row_count, 3u)
        << "visible:\n" << formatHeapScanSummary(visible_heap)
        << "\nraw:\n" << formatHeapScanSummary(raw_heap);

    auto select = compileAndExecute("SELECT id, name FROM copy_shadow_bulk_plan_dst");
    ASSERT_TRUE(select.success()) << select.error();
    ASSERT_TRUE(select.hasResultSet());
    ASSERT_EQ(select.resultSet()->rowCount(), 3u);
    std::vector<std::string> actual_rows;
    for (size_t row = 0; row < select.resultSet()->rowCount(); ++row)
    {
        actual_rows.push_back(select.resultSet()->getValue(row, 0).toString() + ":" +
                              select.resultSet()->getValue(row, 1).toString());
    }
    std::sort(actual_rows.begin(), actual_rows.end());
    const std::vector<std::string> expected_rows = {"1:alpha", "2:beta", "5:pre"};
    EXPECT_EQ(actual_rows, expected_rows);

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, LegacyCopyFromPublishesSortedExactBulkLoadCatalogState) {
    ScopedConfigOverride scoped_sorted_rows("sb.bulk", "sorted_exact_min_rows", "1000");

    auto create_table = compileAndExecute(
        "CREATE TABLE legacy_copy_sorted_bulk_plan_dst (id INT PRIMARY KEY, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    std::string path = makeUniquePath("legacy_copy_sorted_bulk_plan_in", ".csv");
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "id,name\n";
        for (int id = 1000; id >= 1; --id)
        {
            out << id << ",name" << id << "\n";
        }
    }

    std::string sql = "COPY legacy_copy_sorted_bulk_plan_dst FROM '" + path +
                      "' WITH (FORMAT csv, DELIMITER ',', HEADER true)";
    auto copy_in = compileAndExecuteLegacyPg(sql);
    ASSERT_TRUE(copy_in.success()) << copy_in.error();

    ErrorContext ctx;
    std::vector<CatalogManager::BulkLoadPlanCatalogInfo> plans;
    ASSERT_EQ(catalog_->listBulkLoadPlanCatalogEntries(plans, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(plans.size(), 1u);
    EXPECT_EQ(plans.front().ingest_lane, "SORTED_EXACT_BULK");
    EXPECT_EQ(plans.front().phase_state, "COMPLETED");

    auto select = compileAndExecute(
        "SELECT id, name FROM legacy_copy_sorted_bulk_plan_dst ORDER BY id");
    ASSERT_TRUE(select.success()) << select.error();
    ASSERT_TRUE(select.hasResultSet());
    auto* results = select.resultSet();
    ASSERT_EQ(results->rowCount(), 1000u);
    EXPECT_EQ(results->getValue(0, 0).toString(), "1");
    EXPECT_EQ(results->getValue(0, 1).toString(), "name1");
    EXPECT_EQ(results->getValue(999, 0).toString(), "1000");
    EXPECT_EQ(results->getValue(999, 1).toString(), "name1000");

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, LegacyCopyFromShadowLoadRequestPublishesCommittedCutoverState) {
    auto create_table = compileAndExecute(
        "CREATE TABLE legacy_copy_shadow_bulk_plan_dst (id INT PRIMARY KEY, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();
    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO legacy_copy_shadow_bulk_plan_dst VALUES (5, 'pre')").success());

    std::string path = makeUniquePath("legacy_copy_shadow_bulk_plan_in", ".csv");
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "id,name\n";
        out << "2,beta\n";
        out << "1,alpha\n";
    }

    std::string sql = "COPY legacy_copy_shadow_bulk_plan_dst FROM '" + path +
                      "' WITH (FORMAT csv, DELIMITER ',', HEADER true, SHADOW_LOAD true)";
    auto copy_in = compileAndExecuteLegacyPg(sql);
    ASSERT_TRUE(copy_in.success()) << copy_in.error();

    ErrorContext ctx;
    std::vector<CatalogManager::BulkLoadPlanCatalogInfo> plans;
    ASSERT_EQ(catalog_->listBulkLoadPlanCatalogEntries(plans, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(plans.size(), 1u);
    EXPECT_EQ(plans.front().ingest_lane, "SHADOW_LOAD_CUTOVER");
    EXPECT_EQ(plans.front().phase_state, "CUTOVER_COMMITTED");

    std::vector<CatalogManager::BulkLoadEventCatalogInfo> events;
    ASSERT_EQ(catalog_->listBulkLoadEventCatalogEntries(plans.front().bulk_load_plan_uuid, events, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_EQ(events.size(), 3u);
    EXPECT_TRUE(events[0].has_phase_from);
    EXPECT_EQ(events[0].phase_from, "PLANNED");
    EXPECT_EQ(events[0].phase_to, "RUNNING");
    EXPECT_EQ(events[0].event_state, "COMMITTED");
    EXPECT_EQ(events[0].event_code, "COPY_FROM_STARTED");
    EXPECT_TRUE(events[1].has_phase_from);
    EXPECT_EQ(events[1].phase_from, "RUNNING");
    EXPECT_EQ(events[1].phase_to, "CUTOVER_PENDING");
    EXPECT_EQ(events[1].event_state, "COMMITTED");
    EXPECT_EQ(events[1].event_code, "SHADOW_LOAD_CUTOVER_READY");
    EXPECT_TRUE(events[2].has_phase_from);
    EXPECT_EQ(events[2].phase_from, "CUTOVER_PENDING");
    EXPECT_EQ(events[2].phase_to, "CUTOVER_COMMITTED");
    EXPECT_EQ(events[2].event_state, "COMMITTED");
    EXPECT_EQ(events[2].event_code, "SHADOW_LOAD_CUTOVER_COMMITTED");

    CatalogManager::BulkLoadCutoverGuardCatalogInfo guard{};
    ASSERT_EQ(catalog_->getBulkLoadCutoverGuardCatalogEntry(plans.front().bulk_load_plan_uuid, guard, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(guard.guard_state, "CUTOVER_COMMITTED");
    EXPECT_TRUE(guard.dependency_refresh_complete);

    CatalogManager::TableInfo table_info{};
    ASSERT_EQ(catalog_->getTable(public_schema_id_,
                                 "legacy_copy_shadow_bulk_plan_dst",
                                 table_info,
                                 &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_NE(table_info.tablespace_id, PRIMARY_TABLESPACE_ID);

    const auto visible_heap = scanHeapRows(table_info.table_id, false);
    const auto raw_heap = scanHeapRows(table_info.table_id, true);
    EXPECT_EQ(raw_heap.row_count, 3u) << formatHeapScanSummary(raw_heap);
    EXPECT_EQ(visible_heap.row_count, 3u)
        << "visible:\n" << formatHeapScanSummary(visible_heap)
        << "\nraw:\n" << formatHeapScanSummary(raw_heap);

    auto select = compileAndExecute("SELECT id, name FROM legacy_copy_shadow_bulk_plan_dst");
    ASSERT_TRUE(select.success()) << select.error();
    ASSERT_TRUE(select.hasResultSet());
    ASSERT_EQ(select.resultSet()->rowCount(), 3u);
    std::vector<std::string> actual_rows;
    for (size_t row = 0; row < select.resultSet()->rowCount(); ++row)
    {
        actual_rows.push_back(select.resultSet()->getValue(row, 0).toString() + ":" +
                              select.resultSet()->getValue(row, 1).toString());
    }
    std::sort(actual_rows.begin(), actual_rows.end());
    const std::vector<std::string> expected_rows = {"1:alpha", "2:beta", "5:pre"};
    EXPECT_EQ(actual_rows, expected_rows);

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyEncodingUtf8Accepted) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_enc (id INT, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    ASSERT_TRUE(compileAndExecute(
        "INSERT INTO copy_enc VALUES (1, 'alpha')").success());

    std::string path = makeUniquePath("copy_enc_utf8", ".csv");
    std::string sql = "COPY copy_enc TO '" + path +
                      "' WITH (FORMAT csv, ENCODING 'UTF8')";
    auto copy_out = compileAndExecute(sql);
    ASSERT_TRUE(copy_out.success()) << copy_out.error();

    std::ifstream in(path);
    ASSERT_TRUE(in.is_open());
    std::string line;
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_FALSE(line.empty());

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyEncodingUnsupportedRejected) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_enc_bad (id INT, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    std::string path = makeUniquePath("copy_enc_bad", ".csv");
    std::string sql = "COPY copy_enc_bad TO '" + path +
                      "' WITH (FORMAT csv, ENCODING 'LATIN1')";
    auto copy_out = compileAndExecute(sql);
    ASSERT_FALSE(copy_out.success());
    EXPECT_NE(copy_out.error().find("COPY ENCODING"), std::string::npos);

    std::filesystem::remove(path);
}

TEST_F(CopyExecutorTest, CopyFromPreservesWriteFenceStatusAtExecutorBoundary) {
    auto create_table = compileAndExecute(
        "CREATE TABLE copy_fenced (id INT, name VARCHAR(10))");
    ASSERT_TRUE(create_table.success()) << create_table.error();

    std::string path = makeUniquePath("copy_fenced", ".csv");
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "1,alpha\n";
    }

    auto* failpoints = db_.mga_failpoint_manager();
    ASSERT_NE(failpoints, nullptr);

    ErrorContext ctx;
    scratchbird::core::MgaFailpointDefinition definition{};
    definition.trigger_name =
        std::string(scratchbird::core::MgaFailpointTriggers::kWritebackSyncFailure);
    definition.injected_status = Status::DISK_FULL;
    ASSERT_EQ(failpoints->installSeed("tdrw012_copy_write_fence", {definition}, &ctx), Status::OK)
        << ctx.message;

    scratchbird::core::WritebackAttribution attribution{};
    attribution.queue_kind = scratchbird::core::WritebackQueueKind::FOREGROUND_HELP;
    attribution.policy_domain = scratchbird::core::WritebackPolicyDomain::TRANSACTION;
    ASSERT_EQ(db_.sync(&ctx, attribution), Status::DISK_FULL);
    ASSERT_EQ(failpoints->clear(&ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(db_.write_admission_fenced());

    std::string sql = "COPY copy_fenced FROM '" + path +
                      "' WITH (FORMAT csv, DELIMITER ',', BATCH_SIZE 1)";
    auto copy_in = compileAndExecute(sql);
    ASSERT_FALSE(copy_in.success());
    EXPECT_EQ(copy_in.status(), Status::DISK_FULL) << copy_in.error();
    EXPECT_EQ(copy_in.sqlstate(), std::string(statusToSQLState(Status::DISK_FULL)))
        << copy_in.error();
    EXPECT_NE(copy_in.error().find("writeback"), std::string::npos);

    std::filesystem::remove(path);
}
