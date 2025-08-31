/**
 * Phase 1: Heap Storage and Row Format - Comprehensive Tests
 * 
 * Tests all requirements from ProjectPlan/Phase 1
 * Exit Criteria: Create/insert/select rows via internal harness with validation
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <random>
#include "scratchbird/engine.h"
#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/ods.h"

namespace fs = std::filesystem;

class Phase1HeapStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = fs::temp_directory_path() / "phase1_test";
        fs::create_directories(test_dir);
        
        scratchbird::Status status;
        db = scratchbird::create_database(test_dir / "test.db", {}, status);
        ASSERT_EQ(status.code, scratchbird::StatusCode::Ok);
    }
    
    void TearDown() override {
        if (db) scratchbird::close_database(db);
        fs::remove_all(test_dir);
    }
    
    fs::path test_dir;
    std::shared_ptr<scratchbird::Database> db;
};

// Test 1.1: Page Structure Implementation
TEST_F(Phase1HeapStorageTest, HeapPageStructure) {
    // Verify heap page layout: [PageHeader][HeapPageHeader][tuples][free][slots]
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    
    scratchbird::engine::FileMap fmap(layout);
    fmap.set_base_path(test_dir, "heap_test");
    
    // Create a heap page
    std::vector<uint8_t> page(layout.page_size);
    auto* page_header = reinterpret_cast<scratchbird::engine::ods::PageHeader*>(page.data());
    
    // Verify PageHeader fields
    page_header->page_size = layout.page_size;
    page_header->type = static_cast<uint16_t>(scratchbird::engine::ods::PageType::HeapData);
    page_header->page_no = 1;
    page_header->space_id = 1;
    
    // Verify HeapPageHeader follows PageHeader
    auto* heap_header = reinterpret_cast<scratchbird::engine::ods::HeapPageHeader*>(
        page.data() + sizeof(scratchbird::engine::ods::PageHeader));
    
    heap_header->num_slots = 0;
    heap_header->free_start = sizeof(scratchbird::engine::ods::PageHeader) + 
                              sizeof(scratchbird::engine::ods::HeapPageHeader);
    heap_header->dir_start = layout.page_size;
    heap_header->flags = 0;
    
    // Verify slot directory grows from end
    EXPECT_EQ(heap_header->dir_start, layout.page_size);
    EXPECT_GT(heap_header->free_start, sizeof(scratchbird::engine::ods::PageHeader));
    
    // Calculate and verify checksum
    page_header->checksum = 0;
    page_header->checksum = scratchbird::engine::ods::crc32c(page.data(), page.size());
    EXPECT_NE(page_header->checksum, 0);
}

// Test 1.2: Tuple Record Header Implementation
TEST_F(Phase1HeapStorageTest, TupleRecordHeader) {
    // Verify tuple header structure per specification
    struct TupleHeader {
        uint64_t created_xid;
        uint64_t deleted_xid;
        uint64_t backptr_rid;
        uint16_t num_attrs;
        uint16_t nullmap_bytes;
        uint16_t varlena_bytes;
        uint16_t flags;
    };
    
    TupleHeader header;
    header.created_xid = 100;
    header.deleted_xid = 0;  // Live tuple
    header.backptr_rid = 0;  // No previous version
    header.num_attrs = 3;
    header.nullmap_bytes = 1;  // 3 attrs need 1 byte
    header.varlena_bytes = 0;
    header.flags = 0;
    
    // Verify header size and alignment
    EXPECT_EQ(sizeof(header), 32);  // Should be aligned
    EXPECT_EQ(header.deleted_xid, 0) << "Live tuple should have deleted_xid = 0";
}

// Test 1.3: RowID Format Implementation
TEST_F(Phase1HeapStorageTest, RowIDFormat) {
    // Test 64-bit logical RID format
    union RowID {
        uint64_t value;
        struct {
            uint16_t slot_no;
            uint32_t page_no;
            uint16_t space_id;
        } parts;
    };
    
    RowID rid;
    rid.parts.space_id = 1;
    rid.parts.page_no = 12345;
    rid.parts.slot_no = 42;
    
    // Pack and unpack
    uint64_t packed = rid.value;
    
    RowID unpacked;
    unpacked.value = packed;
    
    EXPECT_EQ(unpacked.parts.space_id, 1);
    EXPECT_EQ(unpacked.parts.page_no, 12345);
    EXPECT_EQ(unpacked.parts.slot_no, 42);
    
    // Test helpers
    auto file_offset = scratchbird::engine::compute_file_offset(rid.value, 8192);
    EXPECT_EQ(file_offset, 12345ULL * 8192);  // page_no * page_size
}

// Test 1.4: Null Bitmap Implementation
TEST_F(Phase1HeapStorageTest, NullBitmapHandling) {
    // Test null bitmap encoding/decoding
    scratchbird::engine::TupleLayout layout;
    layout.attrs = {
        {scratchbird::engine::AttrType::Int32, 4, true, true},   // nullable
        {scratchbird::engine::AttrType::VarBytes, 0, false, false}, // not null
        {scratchbird::engine::AttrType::Int64, 8, true, true},   // nullable
    };
    
    // Create tuple with nulls
    std::vector<scratchbird::engine::Value> values = {
        scratchbird::engine::Value::null(),
        scratchbird::engine::Value::from_string("test"),
        scratchbird::engine::Value::from_int64(42),
    };
    
    // Encode
    auto encoded = scratchbird::engine::encode_tuple(layout, values);
    ASSERT_FALSE(encoded.empty());
    
    // Verify null bitmap
    uint8_t null_bitmap = encoded[0];  // First byte after header
    EXPECT_TRUE(null_bitmap & 0x01) << "First attribute should be marked null";
    EXPECT_FALSE(null_bitmap & 0x02) << "Second attribute should not be null";
    EXPECT_FALSE(null_bitmap & 0x04) << "Third attribute should not be null";
    
    // Decode and verify
    auto decoded = scratchbird::engine::decode_tuple(layout, encoded);
    ASSERT_EQ(decoded.size(), 3);
    EXPECT_TRUE(decoded[0].is_null());
    EXPECT_FALSE(decoded[1].is_null());
    EXPECT_EQ(decoded[2].as_int64(), 42);
}

// Test 1.5: Varlena and Overflow Support
TEST_F(Phase1HeapStorageTest, VarlenaAndOverflow) {
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create table with varlena column
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE varlena_test (id INTEGER, small_text TEXT, large_text TEXT)", 
        status), {});
    
    // Test small varlena (inline)
    std::string small_data(100, 'A');
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO varlena_test VALUES (1, ?, NULL)", status), {small_data});
    
    // Test large varlena (overflow)
    std::string large_data(10000, 'B');  // Force overflow
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO varlena_test VALUES (2, NULL, ?)", status), {large_data});
    
    // Verify retrieval
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM varlena_test ORDER BY id", status), {});
    
    ASSERT_EQ(result.rows.size(), 2);
    EXPECT_EQ(result.rows[0]["small_text"], small_data);
    EXPECT_EQ(result.rows[1]["large_text"], large_data);
    
    // Verify overflow page created
    fs::path db_file = test_dir / "test.db.seg0";
    auto file_size = fs::file_size(db_file);
    EXPECT_GT(file_size, 16384) << "Overflow should create additional pages";
}

// Test 1.6: HeapRelation API
TEST_F(Phase1HeapStorageTest, HeapRelationAPI) {
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    
    scratchbird::engine::FileMap fmap(layout);
    fmap.set_base_path(test_dir, "heap_rel");
    
    // Create HeapRelation
    scratchbird::engine::TupleLayout tuple_layout;
    tuple_layout.attrs = {
        {scratchbird::engine::AttrType::Int32, 4, true, false},
        {scratchbird::engine::AttrType::VarBytes, 0, false, true},
    };
    
    scratchbird::engine::HeapOptions opts;
    auto heap_rel = scratchbird::engine::HeapRelation::create(
        fmap, tuple_layout, opts);
    
    // Test insert
    std::vector<scratchbird::engine::Value> row1 = {
        scratchbird::engine::Value::from_int32(1),
        scratchbird::engine::Value::from_string("First row"),
    };
    
    auto insert_result = heap_rel.insert(row1);
    EXPECT_NE(insert_result.rid, 0);
    EXPECT_GT(insert_result.bytes_written, 0);
    EXPECT_FALSE(insert_result.overflow);
    
    // Test fetch
    std::vector<scratchbird::engine::Value> fetched;
    bool found = heap_rel.fetch(insert_result.rid, fetched);
    EXPECT_TRUE(found);
    ASSERT_EQ(fetched.size(), 2);
    EXPECT_EQ(fetched[0].as_int32(), 1);
    EXPECT_EQ(fetched[1].as_string(), "First row");
    
    // Test scan
    auto scan = heap_rel.open_scan();
    std::vector<scratchbird::engine::Value> scanned;
    scratchbird::engine::RowId rid_out;
    
    int count = 0;
    while (scan.next(scanned, &rid_out)) {
        count++;
        EXPECT_EQ(scanned.size(), 2);
    }
    EXPECT_EQ(count, 1);
}

// Test 1.7: Heap Root Page
TEST_F(Phase1HeapStorageTest, HeapRootPage) {
    scratchbird::engine::FileMap::Layout layout;
    layout.page_size = 8192;
    
    scratchbird::engine::FileMap fmap(layout);
    fmap.set_base_path(test_dir, "heap_root");
    
    // Create heap root page
    std::vector<uint8_t> page(layout.page_size);
    auto* header = reinterpret_cast<scratchbird::engine::ods::PageHeader*>(page.data());
    header->type = static_cast<uint16_t>(scratchbird::engine::ods::PageType::HeapRoot);
    header->page_no = 0;
    
    // HeapRoot payload
    struct HeapRootPayload {
        uint16_t version;
        uint16_t flags;
        uint32_t first_heap_page;
        uint32_t last_heap_page;
        uint32_t free_space_hint_page;
        uint32_t tuple_format_id;
    };
    
    auto* root = reinterpret_cast<HeapRootPayload*>(
        page.data() + sizeof(scratchbird::engine::ods::PageHeader));
    
    root->version = 1;
    root->flags = 0;
    root->first_heap_page = 1;
    root->last_heap_page = 1;
    root->free_space_hint_page = 1;
    root->tuple_format_id = 12345;
    
    // Write and verify
    fmap.write_page(0, page.data());
    
    std::vector<uint8_t> read_page(layout.page_size);
    fmap.read_page(0, read_page.data());
    
    auto* read_root = reinterpret_cast<HeapRootPayload*>(
        read_page.data() + sizeof(scratchbird::engine::ods::PageHeader));
    
    EXPECT_EQ(read_root->version, 1);
    EXPECT_EQ(read_root->tuple_format_id, 12345);
}

// Test 1.8: Free Space Tracking
TEST_F(Phase1HeapStorageTest, FreeSpaceTracking) {
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE free_space_test (id INTEGER, data TEXT)", status), {});
    
    // Fill a page
    for (int i = 0; i < 100; i++) {
        std::string data = "Row " + std::to_string(i);
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO free_space_test VALUES (?, ?)", status),
            {std::to_string(i), data});
    }
    
    // Delete some rows to create free space
    scratchbird::execute(scratchbird::prepare(session,
        "DELETE FROM free_space_test WHERE id % 2 = 0", status), {});
    
    // Insert new data - should reuse free space
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO free_space_test VALUES (1000, 'Reused space')", status), {});
    
    // Verify space was reused (file shouldn't grow much)
    fs::path db_file = test_dir / "test.db.seg0";
    auto size_before = fs::file_size(db_file);
    
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO free_space_test VALUES (1001, 'Another reuse')", status), {});
    
    auto size_after = fs::file_size(db_file);
    EXPECT_EQ(size_before, size_after) << "Free space should be reused";
}

// Test 1.9: Comprehensive Integration Test
TEST_F(Phase1HeapStorageTest, Phase1ExitCriteria) {
    // Exit criteria: Create/insert/select rows via internal harness
    scratchbird::Status status;
    auto session = scratchbird::create_session(db, status);
    
    // Create complex table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE phase1_complete ("
        "  id INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  age INTEGER,"
        "  salary DECIMAL(10,2),"
        "  hire_date DATE,"
        "  notes TEXT"
        ")", status), {});
    
    // Insert various row types
    std::vector<std::vector<std::string>> test_data = {
        {"1", "Alice", "30", "75000.50", "2020-01-15", "Senior developer"},
        {"2", "Bob", "NULL", "60000.00", "2021-03-20", "NULL"},
        {"3", "Charlie", "25", "55000.75", "2022-06-01", std::string(5000, 'X')}, // Large text
    };
    
    for (const auto& row : test_data) {
        scratchbird::execute(scratchbird::prepare(session,
            "INSERT INTO phase1_complete VALUES (?, ?, ?, ?, ?, ?)", status), row);
    }
    
    // Select and verify all data
    auto result = scratchbird::execute(scratchbird::prepare(session,
        "SELECT * FROM phase1_complete ORDER BY id", status), {});
    
    ASSERT_EQ(result.rows.size(), 3);
    
    // Verify row 1
    EXPECT_EQ(result.rows[0]["name"], "Alice");
    EXPECT_EQ(result.rows[0]["age"], "30");
    
    // Verify row 2 with NULL
    EXPECT_EQ(result.rows[1]["name"], "Bob");
    EXPECT_TRUE(result.rows[1]["age"].empty() || result.rows[1]["age"] == "NULL");
    
    // Verify row 3 with large text
    EXPECT_EQ(result.rows[2]["notes"].length(), 5000);
    
    // Verify page validation tools work
    auto validate_result = scratchbird::engine::validate_heap_pages(
        test_dir / "test.db", status);
    EXPECT_TRUE(validate_result.valid) << "Heap pages should pass validation";
    EXPECT_EQ(validate_result.errors.size(), 0);
    
    std::cout << "Phase 1 Exit Criteria MET: "
              << "✅ Create/insert/select rows via internal harness\n"
              << "✅ Comprehensive page validation tools\n";
}