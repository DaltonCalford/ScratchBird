# ScratchBird Coding and Build Standards

## MANDATORY: Read This Before Every Implementation Phase

This document defines the **absolute requirements** for all code contributions to ScratchBird. 
**AI Implementers**: Reference this document at the start of EVERY work session.

## Build System Rules (NON-NEGOTIABLE)

### ❌ NEVER DO THIS
```bash
# WRONG - Direct compilation
g++ -o test test.cpp

# WRONG - Manual linking
g++ main.cpp engine.cpp -o scratchbird

# WRONG - Ad-hoc testing
./my_test_program

# WRONG - Modifying CMakeLists.txt without testing
echo "add_executable(test test.cpp)" >> CMakeLists.txt
```

### ✅ ALWAYS DO THIS
```bash
# CORRECT - Use CMake build system
mkdir -p build
cd build
cmake ..
make -j$(nproc)
make test

# CORRECT - Run tests through CTest
ctest --verbose
ctest --output-on-failure

# CORRECT - Add to CMakeLists.txt properly
# Edit src/CMakeLists.txt or tests/CMakeLists.txt
# Then rebuild from build directory
```

## Project Structure Rules

### Source Code Organization
```
/workspace/
├── src/
│   ├── engine/          # Core engine code ONLY
│   │   ├── storage/     # Storage subsystem
│   │   ├── transaction/ # Transaction management
│   │   ├── buffer/      # Buffer management
│   │   ├── catalog/     # System catalog
│   │   └── access/      # Access methods
│   ├── parser/          # SQL parsing ONLY
│   ├── executor/        # Query execution ONLY
│   ├── network/         # Network protocols ONLY
│   └── CMakeLists.txt   # DO NOT CREATE NEW ONES
├── include/
│   └── scratchbird/     # Public headers
│       ├── engine/
│       ├── parser/
│       └── common/
├── tests/
│   ├── phase_X_XX/      # Tests organized by phase
│   └── CMakeLists.txt   # DO NOT CREATE NEW ONES
└── build/               # BUILD OUTPUT ONLY - NEVER COMMIT
```

### File Naming Conventions
```cpp
// Headers
page_manager.h         // lowercase with underscores
buffer_pool.h         // NOT: BufferPool.h, buffer-pool.h

// Implementation
page_manager.cpp      // matches header name exactly
buffer_pool.cpp       // NOT: bufferpool.cpp, BufferPoolImpl.cpp

// Tests  
test_page_manager.cpp // prefix with test_
test_buffer_pool.cpp  // NOT: page_manager_test.cpp
```

## C++ Coding Standards

### Required C++ Version
```cpp
// C++17 is the minimum - use modern features
#include <optional>
#include <variant>
#include <string_view>
#include <filesystem>
```

### Header Guards
```cpp
// ALWAYS use #pragma once for new code
#pragma once

// NOT the old style:
// #ifndef SCRATCHBIRD_ENGINE_PAGE_H
// #define SCRATCHBIRD_ENGINE_PAGE_H
```

### Namespace Rules
```cpp
namespace scratchbird {
namespace engine {

// Implementation here

} // namespace engine
} // namespace scratchbird

// NEVER use 'using namespace' in headers
// using namespace std; // WRONG in .h files
```

### Memory Management
```cpp
// Use smart pointers - no raw new/delete
auto page = std::make_unique<Page>();        // Unique ownership
auto buffer = std::make_shared<Buffer>();    // Shared ownership

// NEVER do this:
Page* page = new Page();  // WRONG
delete page;               // WRONG

// RAII for resources
class FileHandle {
    int fd_;
public:
    FileHandle(const std::string& path) : fd_(open(path.c_str(), O_RDWR)) {
        if (fd_ < 0) throw std::runtime_error("Failed to open file");
    }
    ~FileHandle() { if (fd_ >= 0) close(fd_); }
    // Delete copy, implement move
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
};
```

### Error Handling
```cpp
// Use exceptions for errors
if (!page) {
    throw std::runtime_error("Page allocation failed");
}

// Use optional for maybe-values
std::optional<PageId> findPage(const Key& key);

// Use expected/result for operations that can fail
// expected<T, Error> readPage(PageId id);

// NEVER use error codes as return values
// int readPage(PageId id, Page* out);  // WRONG
```

### Constants and Enums
```cpp
// Use constexpr for compile-time constants
constexpr size_t kPageSize = 8192;
constexpr size_t kBufferPoolSize = 1024 * 1024 * 1024;  // 1GB

// Use enum class, not plain enum
enum class PageType : uint8_t {
    DATA_PAGE = 0x01,
    INDEX_LEAF = 0x02,
    INDEX_INTERNAL = 0x03
};

// NOT:
#define PAGE_SIZE 8192        // WRONG
enum PageType { DATA = 1 };  // WRONG
```

## CMake Integration Rules

### Adding New Source Files
```cmake
# In src/engine/CMakeLists.txt (if it exists)
# Or src/CMakeLists.txt

# Group related files
set(STORAGE_SOURCES
    engine/storage/page_manager.cpp
    engine/storage/heap_file.cpp
    engine/storage/buffer_pool.cpp
)

set(STORAGE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/scratchbird/engine/storage/page_manager.h
    ${CMAKE_SOURCE_DIR}/include/scratchbird/engine/storage/heap_file.h
    ${CMAKE_SOURCE_DIR}/include/scratchbird/engine/storage/buffer_pool.h
)

# Add to library
add_library(scratchbird_storage STATIC
    ${STORAGE_SOURCES}
    ${STORAGE_HEADERS}
)

target_include_directories(scratchbird_storage PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
```

### Adding New Tests
```cmake
# In tests/CMakeLists.txt

# Create test executable
add_executable(test_page_manager
    phase_1_03/test_page_manager.cpp
)

target_link_libraries(test_page_manager
    scratchbird_storage
    gtest_main
)

# Register with CTest
add_test(NAME PageManager COMMAND test_page_manager)

# Set properties
set_tests_properties(PageManager PROPERTIES
    TIMEOUT 30
    LABELS "phase_1_03;storage"
)
```

## Testing Requirements

### Test Organization
```cpp
// tests/phase_1_03/test_page_manager.cpp
#include <gtest/gtest.h>
#include <scratchbird/engine/storage/page_manager.h>

namespace scratchbird {
namespace engine {
namespace test {

class PageManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
    
    void TearDown() override {
        // Cleanup code
    }
};

TEST_F(PageManagerTest, CreatePage) {
    // Test implementation
}

TEST_F(PageManagerTest, AllAlphaPageSizes) {
    // Alpha MUST test 3 page sizes; Beta extends to 5
    for (auto page_size : {8192, 16384, 32768}) {
        // Test with each size
    }
}

} // namespace test
} // namespace engine
} // namespace scratchbird
```

### Test Execution
```bash
# From build directory
make test                  # Run all tests
ctest -R PageManager       # Run specific test
ctest -L phase_1_03        # Run tests by label
ctest --verbose            # Verbose output

# With sanitizers (if enabled)
ctest --output-on-failure

# Never run tests directly
# ./test_page_manager      # WRONG (unless debugging)
```

## Compilation Flags

### Debug Build
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
# Enables: -g -O0 -DDEBUG
```

### Release Build
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
# Enables: -O3 -DNDEBUG
```

### Sanitizer Builds
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..   # AddressSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON ..  # UndefinedBehaviorSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..   # ThreadSanitizer
```

## Code Review Checklist

Before committing ANY code:

- [ ] Code compiles with `cmake && make` (NOT direct g++)
- [ ] All tests pass with `make test`
- [ ] No compiler warnings with `-Wall -Wextra`
- [ ] Memory leaks checked (valgrind or ASAN)
- [ ] Code follows naming conventions
- [ ] Headers use `#pragma once`
- [ ] Smart pointers used (no raw new/delete)
- [ ] CMakeLists.txt updated properly
- [ ] Tests cover all page sizes (8K, 16K, 32K, 64K, 128K)
- [ ] Progress logged in `/workspace/ProjectPlan/progress/implementation.log`

## Common AI Mistakes to Avoid

### 1. Creating Standalone Test Programs
```cpp
// WRONG - Creating random test files
int main() {
    Page p;
    p.test();
    return 0;
}
```

### 2. Direct Compilation
```bash
# WRONG - Compiling directly
g++ -std=c++17 test.cpp -o test
./test
```

### 3. Modifying Build System Incorrectly
```cmake
# WRONG - Creating new CMakeLists.txt files everywhere
# There should be ONE per major directory
```

### 4. Forgetting Namespaces
```cpp
// WRONG - Global namespace pollution
class Page { };  // Should be in scratchbird::engine
```

### 5. Using C-style Code
```cpp
// WRONG - C-style
char* buffer = (char*)malloc(8192);
memset(buffer, 0, 8192);
free(buffer);

// CORRECT - C++ style
std::vector<uint8_t> buffer(8192, 0);
```

## Build Verification Commands

### Quick Build Check
```bash
#!/bin/bash
# Save as: verify_build.sh

set -e  # Exit on error

echo "=== ScratchBird Build Verification ==="

# Clean build
rm -rf build
mkdir build
cd build

# Configure
echo "Configuring..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
echo "Building..."
make -j$(nproc)

# Test
echo "Testing..."
ctest --output-on-failure

echo "=== Build Verification PASSED ==="
```

## Phase Implementation Workflow

### EVERY Phase Must Follow This:

1. **Read Phase Specification**
   ```
   ProjectPlan/Phase_XX_*.md
   ```

2. **Read This Document**
   ```
   references/CODING_AND_BUILD_STANDARDS.md
   ```

3. **Read Technical Specs**
   ```
   references/technical_specifications/*.md
   ```

4. **Create Feature Branch** (optional but recommended)
   ```bash
   git checkout -b phase_1_03_heap_storage
   ```

5. **Implement Following Standards**
   - Use CMake
   - Follow naming conventions
   - Write tests first (TDD)

6. **Build and Test**
   ```bash
   cd build
   cmake ..
   make -j$(nproc)
   ctest --output-on-failure
   ```

7. **Log Progress**
   ```
   echo "2024-01-15 14:00 1.03.1 IMPLEMENTED heap_page.cpp" >> ProjectPlan/progress/implementation.log
   ```

8. **Commit with Meaningful Message**
   ```bash
   git add -A
   git commit -m "Implement Phase 1.03: Heap storage with all page sizes

   - Implemented heap page structure
   - Supports all 5 page sizes (8K-128K)
   - Tests pass with ctest
   - No memory leaks (verified with ASAN)"
   ```

## Final Rules

1. **NO SHORTCUTS** - Always use the build system
2. **NO ASSUMPTIONS** - Check this document
3. **NO LEGACY CODE** - Use modern C++17
4. **NO MEMORY LEAKS** - Use RAII and smart pointers
5. **NO UNTESTED CODE** - Every feature needs tests

## AI Implementation Note

**ATTENTION AI**: At the start of EVERY implementation session:
1. Output: "Reviewing CODING_AND_BUILD_STANDARDS.md"
2. Confirm you will use CMake
3. Confirm you will follow naming conventions
4. Confirm you will test all page sizes
5. Only then begin implementation

This document is **MANDATORY READING** before any code is written!