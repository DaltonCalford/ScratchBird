# Phase 2: Database Lifecycle Management

## Objective
Implement database creation, opening, and closing operations.

## Prerequisites
- Phase 1 complete (core entry point)

## Tasks

### 2.1 Database Structure
Define in `include/scratchbird/engine.h`:
```cpp
struct Database {
    std::string path;
    uint32_t page_size;
    bool is_open;
};
```

### 2.2 Create Database
Implement `create_database()`:
- Create database file at specified path
- Initialize file header with magic number
- Set default page size (8192 bytes)
- Return Database handle

### 2.3 Open Database
Implement `open_database()`:
- Verify file exists
- Check magic number
- Read configuration
- Return Database handle

### 2.4 Close Database
Implement `close_database()`:
- Flush pending writes
- Release file handles
- Clean up resources

## Files to Create/Modify
- `include/scratchbird/engine.h`
- `src/engine/database.cpp`

## Validation Tests
```cpp
// Create and verify database
auto db = create_database("test.db", {}, status);
assert(status.code == StatusCode::Ok);
assert(filesystem::exists("test.db"));

// Open existing database
auto db2 = open_database("test.db", status);
assert(status.code == StatusCode::Ok);

// Close and cleanup
close_database(db);
assert(db->is_open == false);
```

## Exit Criteria
- Database files created with correct structure
- Can open and close databases repeatedly
- Error handling for invalid operations