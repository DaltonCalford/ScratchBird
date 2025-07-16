# ScratchBird v0.6.0 Next Session Plan - GPRE-Free Utility Rewrite

**Date**: July 15, 2025  
**Session Focus**: Modern C++ Utility Implementation  
**Strategic Direction**: Eliminate GPRE dependencies through complete utility rewrite

---

## 🎯 **Session Objectives**

### **Primary Goal**: Implement Modern ScratchBird Client Tools
Complete working client utilities using modern C++ and ScratchBird APIs, eliminating all GPRE preprocessing dependencies.

### **Success Metrics**:
- ✅ **sb_gfix**: Working database maintenance utility (target: 444 lines → ~200 lines modern C++)
- ✅ **sb_gstat**: Working database statistics utility (target: 2,319 lines → ~500 lines modern C++)
- ✅ **Framework**: Reusable utility base class for other tools
- ✅ **Testing**: Verify utilities work with existing ScratchBird server

---

## 📋 **Implementation Priority**

### **Phase 1: Foundation (30 minutes)**
1. **Set up modern C++ development environment**
   ```bash
   # Create new utility development directory
   mkdir -p src/modern_utilities
   cd src/modern_utilities
   
   # Test basic ScratchBird API connectivity
   g++ -std=c++17 -I../include -L../../gen/Release/scratchbird/lib \
       -lsbclient test_connection.cpp -o test_connection
   ```

2. **Create utility base class framework**
   - Connection management
   - Error handling
   - Command line parsing
   - Common utility functions

### **Phase 2: sb_gfix Implementation (45 minutes)**
**Target**: Replace `src/alice/alice_meta.epp` (444 lines) with modern C++

**Core Functionality Analysis**:
```cpp
// Current .epp functionality to modernize:
- Database connection validation
- Database metadata repair
- Transaction cleanup
- Index rebuilding
- Database statistics updates
```

**Implementation Approach**:
```cpp
// sb_gfix_modern.cpp structure:
#include "UtilityBase.h"

class DatabaseMaintenanceTool : public UtilityBase {
public:
    void validateDatabase(const char* database);
    void repairMetadata();
    void cleanupTransactions();
    void rebuildIndexes();
    void updateStatistics();
};
```

### **Phase 3: sb_gstat Implementation (60 minutes)**
**Target**: Replace `src/utilities/gstat/dba.epp` (2,319 lines) with modern C++

**Core Functionality Analysis**:
```cpp
// Current .epp functionality to modernize:
- Database header information
- Table statistics (record counts, page counts)
- Index statistics and analysis
- Transaction information
- System table analysis
```

**Implementation Approach**:
```cpp
// sb_gstat_modern.cpp structure:
#include "UtilityBase.h"

class DatabaseStatsTool : public UtilityBase {
public:
    void showDatabaseHeader();
    void showTableStatistics();
    void showIndexStatistics();
    void showTransactionInfo();
    void showSystemTables();
};
```

### **Phase 4: Testing & Validation (30 minutes)**
1. **Create test database for validation**
2. **Test basic functionality of both tools**
3. **Compare output with legacy tools (if available)**
4. **Document any differences or improvements**

---

## 🔧 **Technical Implementation Details**

### **Development Environment Setup**
```bash
# Required includes and libraries
INCLUDES="-I./src/include -I./src/include/firebird"
LIBS="-L./gen/Release/scratchbird/lib -lsbclient"
CPPFLAGS="-std=c++17 -Wall -Wextra -O2"

# Compilation pattern
g++ $CPPFLAGS $INCLUDES $LIBS source.cpp -o executable
```

### **ScratchBird API Integration**
```cpp
// Modern error handling pattern
try {
    IAttachment* attachment = provider->attachDatabase(status, database, 0, nullptr);
    if (status->getState() & IStatus::STATE_ERRORS) {
        ISC_STATUS_ARRAY statusVector;
        status->getErrors(statusVector);
        throw DatabaseException(statusVector);
    }
    
    // Use attachment for database operations
    IStatement* stmt = attachment->prepare(status, transaction, 0, sql, 3, nullptr);
    // ... execute and process results
    
} catch (const DatabaseException& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
    return 1;
}
```

### **Command Line Interface Design**
```bash
# sb_gfix command line interface
sb_gfix [options] <database>
  -v, --validate     Validate database integrity
  -r, --repair       Repair database metadata
  -i, --indexes      Rebuild all indexes
  -s, --statistics   Update database statistics
  -h, --help         Show this help message

# sb_gstat command line interface  
sb_gstat [options] <database>
  -h, --header       Show database header information
  -t, --tables       Show table statistics
  -i, --indexes      Show index statistics
  -s, --system       Show system table information
  -a, --all          Show all statistics (default)
```

---

## 🧪 **Testing Strategy**

### **Test Database Setup**
```sql
-- Create test database for validation
CREATE DATABASE 'test_utilities.fdb' 
USER 'SYSDBA' PASSWORD 'masterkey'
DEFAULT CHARACTER SET UTF8;

-- Create test schema with hierarchical structure
CREATE SCHEMA test;
CREATE SCHEMA test.sub1;
CREATE SCHEMA test.sub1.sub2;

-- Create test tables
CREATE TABLE test.sub1.sub2.sample_table (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    created_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert test data
INSERT INTO test.sub1.sub2.sample_table (id, name) VALUES (1, 'Test Record');
```

### **Functionality Verification**
```bash
# Test sb_gfix
./sb_gfix --validate test_utilities.fdb
./sb_gfix --statistics test_utilities.fdb

# Test sb_gstat  
./sb_gstat --header test_utilities.fdb
./sb_gstat --tables test_utilities.fdb
./sb_gstat --all test_utilities.fdb
```

---

## 📊 **Expected Outcomes**

### **Immediate Results**
- **2 Working Utilities**: sb_gfix and sb_gstat fully functional
- **Modern Codebase**: Clean C++ code with no GPRE dependencies
- **Build System**: No preprocessing required, standard compilation
- **Testing**: Verified against real ScratchBird databases

### **Strategic Impact**
- **Eliminates BLOCKER #7**: No more GPRE preprocessing issues
- **Modernizes Codebase**: Sets pattern for remaining utility rewrites
- **Improves Maintainability**: Standard debugging and testing tools work
- **Accelerates Development**: Faster iteration cycles for future work

---

## 🔄 **Session Workflow**

### **Time Allocation (3 hours)**
- **00:00-00:30**: Environment setup and base class framework
- **00:30-01:15**: sb_gfix implementation and testing
- **01:15-02:15**: sb_gstat implementation and testing
- **02:15-02:45**: Integration testing and validation
- **02:45-03:00**: Documentation and next steps planning

### **Deliverables**
1. **src/modern_utilities/UtilityBase.h** - Common utility framework
2. **src/modern_utilities/sb_gfix_modern.cpp** - Database maintenance tool
3. **src/modern_utilities/sb_gstat_modern.cpp** - Database statistics tool
4. **src/modern_utilities/Makefile** - Build system for modern utilities
5. **test_results.md** - Validation and testing results

---

## 🚀 **Future Session Preparation**

### **Next Session Targets**
1. **sb_gsec**: Security management utility
2. **sb_gbak**: Backup/restore utility (complex - may need multiple sessions)
3. **sb_isql**: Interactive SQL client (most complex - will need multiple sessions)

### **Documentation Updates**
- Update COMPILATION_METHODS.md with modern utility build process
- Update RELEASE_BLOCKERS.md with resolved status
- Update TECHNICAL_DEBT.md with eliminated GPRE dependencies

This plan eliminates the most significant technical debt (GPRE dependencies) while delivering immediate value through working client tools.