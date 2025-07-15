# ScratchBird v0.6 Technical Debt & Implementation Notes

**Generated**: July 15, 2025  
**Purpose**: Technical details for developers working on release blockers  
**Related**: See `RELEASE_BLOCKERS.md` for high-level summary

---

## 🔧 Technical Implementation Details

### **CRITICAL BLOCKER #1: Database Links Backup/Restore**

**File**: `src/burp/backup.epp:4278-4341`  
**Current State**: Complete functionality commented out

```cpp
// TODO: Re-enable when RDB$DATABASE_LINKS table is available in runtime database
// Database links backup functionality temporarily disabled

/*
// Original implementation was:
if (relation->rel_id == rel_database_links)
{
    // Backup database link metadata
    // Including schema_name, remote_schema, schema_mode, schema_depth
    backup_database_link_record(action, relation, record, tdbb);
    return;
}
*/
```

**Technical Requirements**:
1. Verify `RDB$DATABASE_LINKS` table exists in runtime database
2. Add fields: `RDB$LINK_SCHEMA_NAME`, `RDB$LINK_REMOTE_SCHEMA`, `RDB$LINK_SCHEMA_MODE`, `RDB$LINK_SCHEMA_DEPTH`
3. Re-enable backup functions: `backup_database_link_record()`
4. Re-enable restore functions in `src/burp/restore.epp:11726-11733`

**Dependencies**: Database schema must be updated first

---

### **CRITICAL BLOCKER #2: Schema Display in ISQL**

**File**: `src/isql/show.epp`  
**Current State**: All functions return hardcoded error messages

**Affected Functions**:
```cpp
// Line 2385
static void show_schemas(const bool listAll)
{
    // TODO: Implement show_schemas function
    isqlGlob.printf("Schema functionality temporarily disabled%s", NEWLINE);
}

// Line 2429
static void show_schema()
{
    // TODO: Fix GPRE compilation
    isqlGlob.printf("Current schema functionality temporarily disabled%s", NEWLINE);
}

// Line 2436
static void show_home_schema()
{
    // TODO: Fix GPRE compilation
    isqlGlob.printf("Home schema functionality temporarily disabled%s", NEWLINE);
}

// Line 3599
static void show_database_links()
{
    // TODO: Re-enable when RDB$DATABASE_LINKS table is available
    isqlGlob.printf("DATABASE LINKS feature not yet available in this build%s", NEWLINE);
}
```

**Technical Requirements**:
1. Fix GPRE compilation issues preventing schema queries
2. Implement SQL queries against `RDB$SCHEMAS` table
3. Handle hierarchical schema display (parent/child relationships)
4. Add proper error handling for missing schema context
5. Implement database link display queries

**Root Cause**: GPRE compilation errors when processing schema-related SQL statements

---

### **CRITICAL BLOCKER #3: SchemaPathCache Implementation**

**File**: `src/jrd/SchemaPathCache.cpp`  
**Current State**: 13-line stub file

```cpp
/*
 * Schema Path Cache - Stub implementation for compilation compatibility
 * Temporarily disabled for core build completion
 */

#include "firebird.h"
#include "../jrd/SchemaPathCache.h"

namespace Jrd {

// Placeholder implementation - all functionality stubbed out for now

} // namespace Jrd
```

**Required Implementation**:
1. **Path Parsing**: Split schema paths like `finance.accounting.reports`
2. **Component Caching**: Cache individual schema components
3. **Depth Calculation**: Optimize schema depth calculations
4. **Memory Management**: Efficient allocation/deallocation of cache entries
5. **Thread Safety**: Multi-threaded access to cache

**Performance Impact**: Without this cache, every schema path operation requires string parsing and validation, causing significant performance degradation for deep hierarchies.

---

### **MAJOR BLOCKER #4: DatabaseLink Core Methods**

**File**: `src/jrd/DatabaseLink.cpp`  
**Current State**: Methods stubbed out with TODO comments

**Problematic Methods**:
```cpp
void LinkManager::clearLinks()
{
    ScratchBird::WriteLockGuard guard(linksLock, "LinkManager::clearLinks");
    
    // TODO: Implement proper iterator cleanup
    links.clear();
}

DatabaseLink* LinkManager::findLinkBySchema(const ScratchBird::string& schemaPath)
{
    ScratchBird::ReadLockGuard guard(linksLock, "LinkManager::findLinkBySchema");
    
    // TODO: Implement schema-based link lookup
    return nullptr;
}

bool LinkManager::validateAllLinks(Jrd::thread_db* tdbb)
{
    ScratchBird::ReadLockGuard guard(linksLock, "LinkManager::validateAllLinks");
    
    // TODO: Implement link validation
    return true;
}
```

**Technical Issue**: GenericMap Iterator compatibility problems
```cpp
// Original problematic code:
for (ScratchBird::GenericMap<...>::iterator it = links.begin(); it != links.end(); ++it) {
    delete it->second;  // ERROR: operator-> not available
}

// Need to use:
for (auto it = links.begin(); it != links.end(); ++it) {
    delete (*it).second;  // Correct syntax
}
```

**Fix Requirements**:
1. Resolve GenericMap iterator syntax issues
2. Implement proper link cleanup in `clearLinks()`
3. Add schema-based link lookup algorithm
4. Implement link validation logic
5. Fix LinkIterator methods

---

### **MAJOR BLOCKER #5: ISQL String API Compatibility**

**Files**: `src/isql/FrontendParser.cpp`, `src/isql/isql.epp`  
**Current State**: Multiple functions disabled due to string API issues

**Root Cause**: ScratchBird::string vs std::string compatibility problems
```cpp
// ERROR: ScratchBird::string missing methods
ScratchBird::string str;
str.back();      // Method not available
str.pop_back();  // Method not available
str += another;  // Operator not available for string types
```

**Affected Functions**:
- Schema command parsing
- Database link command parsing  
- Advanced ISQL command processing

**Fix Requirements**:
1. Extend ScratchBird::string class with missing methods
2. Add operator overloads for string concatenation
3. Provide compatibility layer for std::string operations
4. Fix template compilation issues

---

### **MAJOR BLOCKER #6: DatabaseLink DDL Operations**

**File**: `src/dsql/DatabaseLinkNodes.cpp`  
**Current State**: "Full database link functionality is disabled in this build"

**Missing DDL Operations**:
- `CREATE DATABASE LINK`
- `DROP DATABASE LINK`
- `ALTER DATABASE LINK`

**Technical Requirements**:
1. Enable DDL node compilation
2. Implement database link creation logic
3. Add link removal functionality
4. Implement link modification operations
5. Add proper error handling and validation

---

### **CRITICAL BLOCKER #7: GPRE Preprocessing Hang**

**Files**: `src/jrd/Function.epp`, `src/isql/extract.epp`, multiple client tool .epp files  
**Current State**: GPRE preprocessor hangs indefinitely on specific .epp files

**Technical Details**:
```bash
# Hangs indefinitely:
/path/to/gpre_current -n -z -gds_cxx -ids src/jrd/Function.epp temp/Function.cpp
/path/to/gpre_current -m -z -n -ocxx src/isql/extract.epp temp/extract.cpp
```

**Impact**:
- Cannot build client tools (sb_isql, sb_gbak, sb_gfix, sb_gsec, sb_gstat)
- Cannot complete engine build
- Blocks entire client tool compilation pipeline

**Technical Requirements**:
1. Debug GPRE hanging issue with specific .epp files
2. Identify root cause of preprocessing failure
3. Fix or provide workaround for affected files
4. Ensure all .epp files can be processed successfully
5. Complete client tool build pipeline

---

### **MAJOR BLOCKER #8: Incomplete ScratchBird Branding**

**Files**: Multiple build system files and executable targets  
**Current State**: Inconsistent naming across built tools

**Affected Tools and Required Changes**:
```bash
# Current → Required
fb_config → sb_config
fb_lock_print → sb_lock_print
fbguard → sb_guard
fbsvcmgr → sb_svcmgr
fbtracemgr → sb_tracemgr
```

**Technical Requirements**:
1. **Build System Updates**:
   - Update `gen/Makefile` executable targets
   - Update `gen/make.rules` naming conventions
   - Update `gen/make.defaults` path variables

2. **Source File Updates**:
   - Update `src/utilities/guard/` build targets
   - Update `src/utilities/fbsvcmgr/` build targets
   - Update `src/utilities/fbtracemgr/` build targets
   - Update `src/lock/` build targets

3. **Configuration Updates**:
   - Update `fb_config` script to `sb_config`
   - Update version strings and help text
   - Update man pages and documentation references

4. **Installation Scripts**:
   - Update `install.sh` to reference correct executable names
   - Update `FirebirdUninstall.sh` to `ScratchBirdUninstall.sh`
   - Update service management scripts

**Implementation Example**:
```makefile
# In gen/Makefile, change:
fbguard: $(FBGUARD_Objects)
# To:
sb_guard: $(SB_GUARD_Objects)
```

---

## 🔄 Build System Issues

### **GPRE Compilation Problems**
- **Issue**: GPRE fails to process .epp files with schema-related SQL
- **Files Affected**: `src/isql/show.epp`, `src/burp/backup.epp`
- **Symptom**: GPRE preprocessing hangs or fails
- **Root Cause**: Schema-related SQL statements not recognized by GPRE

### **String API Compatibility**
- **Issue**: ScratchBird::string missing standard methods
- **Impact**: Client tools cannot compile with advanced string operations
- **Files Affected**: All ISQL source files
- **Fix Required**: Extend string class or provide compatibility layer

### **Iterator Compatibility**
- **Issue**: GenericMap iterators don't follow standard STL patterns
- **Impact**: Database link code cannot iterate through link collections
- **Files Affected**: `src/jrd/DatabaseLink.cpp`
- **Fix Required**: Use correct iterator access syntax

---

## 🛠️ Implementation Recommendations

### **Phase 1: Enable Basic Functionality**
1. **Fix GPRE Issues**: Resolve schema SQL compilation problems
2. **Enable ISQL Display**: Implement basic schema display functions
3. **Fix String API**: Add missing string methods

### **Phase 2: Complete Core Features**
1. **Database Link Methods**: Complete iterator-based implementations
2. **SchemaPathCache**: Implement performance optimization layer
3. **Backup/Restore**: Re-enable database link persistence

### **Phase 3: Advanced Features**
1. **DDL Operations**: Enable database link DDL statements
2. **Range Types**: Complete range type implementation
3. **Performance Tuning**: Optimize for deep schema hierarchies

---

## 🧪 Testing Requirements

### **Critical Tests Needed**:
1. **Schema Display**: Verify ISQL can show hierarchical schemas
2. **Database Links**: Test backup/restore preserves link configurations
3. **Performance**: Benchmark schema path operations with cache
4. **DDL Operations**: Verify CREATE/DROP DATABASE LINK works
5. **Iterator Safety**: Test GenericMap iterator operations

### **Test Scenarios**:
```sql
-- Schema hierarchy display
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;
SHOW SCHEMAS;

-- Database link operations
CREATE DATABASE LINK finance_link TO 'server:db' SCHEMA_MODE FIXED;
SHOW DATABASE LINKS;
-- Backup/restore should preserve link

-- Performance test
-- Create 8-level deep schema hierarchy
-- Measure path resolution time with/without cache
```

---

## 📊 Risk Assessment

### **High Risk**:
- **Schema Display**: Core user functionality completely broken
- **Database Links**: Data integrity risk due to backup/restore failure
- **Performance**: Unacceptable performance with deep hierarchies

### **Medium Risk**:
- **DDL Operations**: Advertised features non-functional
- **String API**: Client tools unstable

### **Low Risk**:
- **Range Types**: Advanced features incomplete
- **2PC External**: Specialized functionality missing

---

## 🔗 Cross-Component Dependencies

### **Database Schema → Client Tools**
- ISQL display functions depend on schema table structure
- Backup/restore depends on database link table existence

### **Engine → Client Tools**
- String API compatibility affects all client tools
- Iterator compatibility affects database link operations

### **Performance → User Experience**
- Missing SchemaPathCache affects all schema operations
- Deep hierarchies may cause timeouts without optimization

---

## 📝 Developer Notes

### **Build Order**:
1. Fix database schema issues first
2. Resolve GPRE compilation problems
3. Enable client tool functionality
4. Add performance optimizations

### **Testing Strategy**:
1. Unit tests for individual components
2. Integration tests for cross-component functionality
3. Performance tests for schema operations
4. End-to-end tests for complete workflows

### **Code Quality**:
- Remove all TODO comments as functionality is implemented
- Add proper error handling and logging
- Ensure thread safety for concurrent operations
- Document all public APIs

---

**Last Updated**: July 15, 2025  
**Status**: 🔴 ACTIVE DEVELOPMENT REQUIRED  
**Next Review**: After each blocker resolution