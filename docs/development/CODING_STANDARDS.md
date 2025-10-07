# ScratchBird Coding Standards

**Last Updated:** October 6, 2025
**Status:** Actual practices documented (with inconsistencies noted)

This document describes both the **intended** coding standards and the **actual current state** of the codebase as revealed by comprehensive code audit.

---

## 1. Naming Conventions

### Intended Standard

- **Classes and Structs:** `PascalCase`
- **Functions and Methods:** `camelCase`
- **Variables:** `snake_case`
- **Constants and Enums:** `UPPER_CASE_SNAKE_CASE`
- **Private Members:** `snake_case_` (with a trailing underscore)

### Current Reality ⚠️

**INCONSISTENT** - Mixed naming styles across subsystems:

**Core subsystem:**
```cpp
class PageManager { };            // PascalCase ✓
auto allocatePage(...) -> Status; // camelCase ✓
uint32_t total_pages_;            // snake_case ✓
Status::PAGE_FULL                 // SCREAMING_SNAKE_CASE ✓
```

**Parser subsystem:**
```cpp
class Lexer { };                 // PascalCase ✓
Token nextToken();               // camelCase ✓
std::string_view input_;         // snake_case ✓
TokenType::KW_CREATE             // Mixed: PascalCase_SCREAMING ⚠️
```

**Issues Found:**
- Enum naming inconsistent (some `SCREAMING_SNAKE`, some `PascalCase_SCREAMING`)
- Some local variables use camelCase instead of snake_case
- Mix of styles between older and newer code

**Recommendation:** Run clang-format with consistent configuration across entire codebase. See [TODO.md MED-001](TODO.md#med-001-standardize-naming-conventions).

---

## 2. Formatting and Style

### Standard

- **Indentation:** 4 spaces (no tabs)
- **Line Breaks:** Unix-style line endings (LF)
- **Line Length:** Soft limit of 120 characters (not enforced)
- **Braces:** Opening brace on same line for functions/classes
- **Comments:** Use `//` for single-line comments and `/* */` for multi-line comments. Doxygen-style comments are encouraged for public APIs.

### Current Reality ⚠️

**MOSTLY CONSISTENT** with some issues:

**Issues Found:**
- Some files have trailing whitespace
- Inconsistent comment styles throughout:
  ```cpp
  // Single line comment (common)
  /* Block comment */ (rare)
  /** Doxygen-style comment */ (some headers)
  // FIXME: comment (scattered)
  // TODO: comment (42+ instances)
  // TODO(category): comment (some instances)
  ```

**Comment Style Issues:**
- Not all public APIs have Doxygen comments
- Some headers fully documented, others not
- Implementation comments vary in quality

**Recommendation:**
- Standardize on Doxygen (`/** */` or `///`) for all public APIs
- Use `//` for implementation comments
- Convert TODOs to GitHub issues (see TODO.md LOW-001)

---

## 3. Error Handling

### Standard

The project uses a `Status` and `ErrorContext` based error handling mechanism. Exceptions should not be used for control flow.

- All functions that can fail should return a `Status` enum.
- For functions that need to return a value, the value should be returned via an output parameter or use trailing return type with Status.
- The `ErrorContext` struct should be used to provide detailed error information, including the file, line number, and a descriptive error message.
- The `SET_ERROR_CONTEXT` macro should be used to set the error context.

### Current Reality ⚠️

**INCONSISTENT** - Critical issue identified:

**Pattern 1:** Check ctx before use (safe)
```cpp
if (ctx != nullptr) {
    SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "No space");
}
```

**Pattern 2:** Assume ctx is valid (UNSAFE - will crash if nullptr)
```cpp
SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "No space");  // CRASH if ctx is null
```

**Pattern 3:** Optional ErrorContext parameter
```cpp
auto insertTuple(..., ErrorContext *ctx = nullptr) -> Status;
```

**Pattern 4:** Return status without setting context
```cpp
return Status::PAGE_FULL;  // No error details
```

**Issues Found:**
- Inconsistent nullptr checking across subsystems
- Some functions allow nullptr ctx, others require it
- No documentation on which pattern to use when
- Potential for segmentation faults if nullptr passed incorrectly

**CRITICAL:** This inconsistency is a HIGH priority fix. See [TODO.md CRIT-003](TODO.md#crit-003-standardize-error-handling-pattern).

**Required Decision:**
Choose ONE pattern and apply consistently:
- **Option A:** ErrorContext is always required (never nullptr) - simpler, safer
- **Option B:** ErrorContext is optional, always check before use - more flexible

**Recommendation:** Create `SAFE_SET_ERROR_CONTEXT` macro that checks before use:
```cpp
#define SAFE_SET_ERROR_CONTEXT(ctx, status, msg) \
    do { if ((ctx) != nullptr) { SET_ERROR_CONTEXT(ctx, status, msg); } } while(0)
```

---

## 4. Resource Management

### Standard

- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) to manage dynamic memory whenever possible.
- If manual memory management is necessary, use `new(std::nothrow)` and check for `nullptr` to handle allocation failures.
- All resources (memory, file handles, etc.) must be properly released in all code paths, including error paths.
- Follow RAII (Resource Acquisition Is Initialization) principles.

### Current Reality ⚠️

**MOSTLY GOOD** with some exceptions:

**Good Examples (90% of code):**
```cpp
std::unique_ptr<PageManager> page_manager_;
std::unique_ptr<BufferPool> buffer_pool_;
auto page_buffer = std::make_unique<uint8_t[]>(page_size);
```

**Bad Examples (found in lock_manager.cpp):**
```cpp
LockRequest* req = lock_obj->wait_queue_head;
while (req) {
    LockRequest* next = req->next;
    delete req;  // Manual delete
    req = next;
}
delete lock_obj;  // Manual delete
```

**Issues Found:**
- `lock_manager.cpp` uses manual `new`/`delete` throughout
- Prone to leaks if shutdown isn't called
- No exception safety in lock manager
- Raw pointer usage in page manipulation (necessary but risky)

**Recommendation:** Convert lock_manager to use smart pointers. See [TODO.md HIGH-005](TODO.md#high-005-convert-lock-manager-to-raii).

---

## 5. C++ Best Practices

### Standard

- Use modern C++ features (C++17) where appropriate.
- Prefer `enum class` over `enum`.
- Use `const` and `constexpr` where possible.
- Avoid raw pointers when ownership is involved. Use smart pointers instead.
- Write small, focused functions and classes.
- Keep the code clean, readable, and maintainable.
- Use `[[nodiscard]]` for functions whose return value should not be ignored.

### Current Reality ✅⚠️

**MOSTLY GOOD** with areas for improvement:

**Good Practices Found:**
```cpp
enum class Status { OK, ERROR, PAGE_FULL, ... };  // ✓ enum class
[[nodiscard]] auto isValid(...) const -> bool;   // ✓ nodiscard
constexpr uint32_t PAGE_SIZE = 8192;             // ✓ constexpr
```

**Issues Found:**

1. **Const Correctness:** Not consistently applied
   ```cpp
   // Good
   auto getStats() const -> Stats;

   // Bad - could be const but isn't
   auto getSomeValue() -> uint32_t;  // Doesn't modify state
   ```

2. **Magic Numbers:** Many hardcoded constants
   ```cpp
   uint32_t last_page = 100;            // Should be named constant
   header->max_connections = 1;         // Should be config
   constexpr uint32_t MAX_CHAIN = 100;  // Good
   ```

3. **Include Guards:** Mixed styles
   ```cpp
   #pragma once                    // Modern (most files)
   #ifndef FOO_H / #define FOO_H  // Traditional (some files)
   ```

**Recommendations:**
- Add const correctness throughout (TODO.md MED-002)
- Move magic numbers to config.h (TODO.md HIGH-003)
- Standardize on `#pragma once` for all headers

---

## 6. Type Safety and Casts

### Standard

- Avoid C-style casts.
- Use C++ casts (`static_cast`, `dynamic_cast`, `const_cast`, `reinterpret_cast`) appropriately.
- Document why a cast is necessary when using `reinterpret_cast` or `const_cast`.
- Prefer compile-time checks over runtime checks.

### Current Reality ⚠️

**HEAVY CAST USAGE** - Audit found 676+ casts:

**Cast Types Found:**
- `reinterpret_cast<uint8_t*>` - Very common (page manipulation)
- `reinterpret_cast<PageHeader*>` - Common (page headers)
- `static_cast<uint32_t>` - Common (type conversions)
- `const_cast<uint8_t*>` - Rare (1 found - needs review)

**Example of Concerning Pattern:**
```cpp
// database.cpp:756 - Unsafe const_cast
auto *page = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buffer));
auto *header = reinterpret_cast<PageHeader*>(page);
header->checksum = calculatePageChecksum(page, page_size_);
```

**Issues:**
- Potential undefined behavior if buffer is actually const
- Type safety violations
- Difficult to track ownership and lifetime
- No static assertions for struct sizes/alignment

**Recommendations:**
- Audit all const_cast usage (only 1 found)
- Add static assertions:
  ```cpp
  static_assert(sizeof(PageHeader) == 64, "PageHeader size mismatch");
  static_assert(alignof(PageHeader) == 8, "PageHeader alignment");
  ```
- Consider using `std::byte*` instead of `uint8_t*` for type-punning
- Document ownership model for page buffers
- See [TODO.md MED-004](TODO.md#med-004-audit-type-casts-for-safety)

---

## 7. Logging and Debugging

### Standard (New)

**Not previously documented - now standardized:**

- Use `DEBUG_LOG` macro from `debug.h` for debug output
- Use proper logging framework (not fprintf)
- Log levels: DEBUG, INFO, WARN, ERROR, FATAL
- Thread-safe logging required
- Structured log format with timestamps

### Current Reality ❌

**NO LOGGING FRAMEWORK** - Uses fprintf(stderr):

**Current Anti-Pattern:**
```cpp
fprintf(stderr, "[ERROR] Invalid xmin %lu\n", xmin);
fprintf(stderr, "Transaction state: %d\n", state);
// 20+ similar calls throughout codebase
```

**Issues:**
- No log levels
- Output goes to stderr, not log file
- No timestamps
- Not thread-safe
- No structured logging
- No log rotation

**Commented-out debug code found:**
```cpp
// fprintf(stderr, "TransactionManager::initialize() called\n");
// fprintf(stderr, "About to allocate TIP page\n");
// Should be removed or converted to DEBUG_LOG
```

**Recommendations:**
- Implement logging framework (TODO.md HIGH-004)
- Replace all fprintf(stderr) with proper logging
- Remove commented-out debug code (TODO.md MED-003)
- Use DEBUG_LOG for development debugging

---

## 8. Header Organization

### Standard (New)

**Header files should:**
- Use `#pragma once` (preferred) or traditional include guards
- Minimize includes (use forward declarations where possible)
- Include in order: system headers, third-party headers, project headers
- Keep public API separate from implementation details
- Document all public functions with Doxygen comments

### Current Reality ⚠️

**MOSTLY GOOD** with issues:

**Issues Found:**
1. **Duplicate Include** (CRITICAL):
   ```cpp
   // include/scratchbird/core/database.h:10-11
   #include "scratchbird/core/storage_engine.h"
   #include "scratchbird/core/storage_engine.h"  // DUPLICATE!
   ```

2. **Mixed Include Guard Styles:**
   ```cpp
   #pragma once                    // Most files
   #ifndef SCRATCHBIRD_FOO_H      // Some files
   ```

3. **Circular Dependencies:** Some headers have circular includes
4. **Missing Forward Declarations:** Could reduce compile times

**Recommendations:**
- Fix duplicate include immediately (TODO.md CRIT-001)
- Standardize on `#pragma once`
- Add forward declarations to reduce includes
- Break circular dependencies

---

## 9. Testing Standards

### Standard (New)

**Tests should:**
- Use Google Test framework
- Follow Arrange-Act-Assert pattern
- Test one thing per test case
- Use descriptive test names: `SubsystemName_FunctionName_Scenario`
- Clean up resources in teardown
- Use ErrorContext in all operations

### Current Reality ✅⚠️

**GOOD STRUCTURE** with gaps:

**Good Practices:**
```cpp
TEST(HeapPageTest, InsertTuple_BasicOperation) {
    // Arrange
    Database db;
    ErrorContext ctx;

    // Act
    Status status = db.insertTuple(..., &ctx);

    // Assert
    EXPECT_EQ(status, Status::OK);
}
```

**Issues:**
- Some tests missing ErrorContext
- Error path testing incomplete
- Concurrency tests missing (blocked by ConnectionContext)
- Security tests missing
- Boundary tests incomplete

**Test Status:**
- Integration: 3/3 passing ✓
- Unit: Majority passing with known failures
- Known failures: 27+ tests (lexer, parser, edge cases)

**Recommendations:**
- Add error path tests (TODO.md TEST-002)
- Add boundary tests (TODO.md TEST-003)
- Add security tests (TODO.md TEST-004)
- Add concurrency tests when ConnectionContext available (TODO.md TEST-001)

---

## 10. Concurrency and Thread Safety

### Standard (New)

**Thread-safe code should:**
- Use std::mutex for shared data protection
- Follow lock ordering to prevent deadlocks
- Use RAII lock guards (std::lock_guard, std::unique_lock)
- Document thread-safety guarantees
- Use thread-local storage for per-connection context

### Current Reality ❌

**NOT THREAD-SAFE** - Critical missing infrastructure:

**Critical Issue:**
```cpp
// TODO(concurrency): Get proc_id from thread-local storage
// For now, locking is disabled (requires connection context refactoring)
// This appears in 15+ locations!
```

**Issues:**
- No ConnectionContext implementation
- No thread-local storage for proc_id
- Locking DISABLED in many critical sections
- ProcArray initialized but underutilized
- Lock manager functional but bypassed

**Files Affected:**
- `src/core/storage_engine.cpp` (5+ locations)
- `src/core/btree.cpp` (6+ locations)
- `src/core/catalog_manager.cpp` (multiple locations)

**Current State:** Single-connection only, multi-connection support incomplete

**Recommendations:**
- Implement ConnectionContext IMMEDIATELY (TODO.md CRIT-002)
- Enable locking once context available
- Add multi-connection stress tests
- Document thread-safety guarantees per class

---

## 11. Performance Considerations

### Standard (New)

**Performance-conscious code should:**
- Prefer stack allocation over heap
- Use move semantics for large objects
- Avoid unnecessary copies
- Use `std::string_view` for non-owning string references
- Profile before optimizing
- Document performance-critical sections

### Current Reality ✅

**GOOD PERFORMANCE AWARENESS:**

**Good Practices Found:**
```cpp
std::string_view input_;        // ✓ Non-owning reference
auto tuple = std::move(data);   // ✓ Move semantics
constexpr uint32_t PAGE_SIZE;   // ✓ Compile-time constant
```

**Performance Features:**
- Buffer pool with LRU eviction
- Page-level caching (32 pages default)
- Efficient B-tree and hash indexes
- TOAST for large values
- Compression support (LZ4)

**No Major Performance Issues Found**

---

## 12. Documentation Requirements

### Standard

**All code should have:**
- File-level comment describing purpose
- Class-level Doxygen comments for public classes
- Function-level Doxygen comments for public functions
- Complex algorithms should have explanatory comments
- TODOs should reference issue numbers

### Current Reality ⚠️

**INCONSISTENT DOCUMENTATION:**

**Well-Documented Files:**
- Core headers have good comments
- Type system well documented
- Transaction manager explained

**Poorly-Documented Files:**
- Some implementation files lack comments
- Complex algorithms not always explained
- TODOs don't reference issues (42+ markers)

**Recommendations:**
- Add Doxygen to all public APIs (TODO.md LOW-003)
- Generate HTML documentation
- Convert TODOs to GitHub issues (TODO.md LOW-001)
- Document complex algorithms

---

## Summary of Current State

### ✅ What's Good
- RAII and smart pointers used extensively
- Modern C++ features (C++17)
- Good error context propagation system
- Comprehensive validation and bounds checking
- Performance-conscious design
- Good test structure

### ⚠️ What Needs Improvement
- **CRITICAL:** Inconsistent error handling (nullptr safety)
- **CRITICAL:** Missing ConnectionContext/thread-local storage
- **HIGH:** Manual memory management in lock_manager
- **MEDIUM:** Inconsistent naming conventions
- **MEDIUM:** Magic numbers throughout
- **MEDIUM:** Missing const correctness

### ❌ What's Missing
- Logging framework (uses fprintf)
- Thread safety infrastructure
- Complete const correctness
- Standardized comment style
- Documentation generation

---

## Enforcement

### Current State
- No pre-commit hooks
- No CI/CD checks
- Manual code review
- clang-format available but not enforced

### Recommended Enforcement
1. Add pre-commit hook for clang-format
2. Add CI checks for:
   - Coding style
   - Unit test pass rate
   - Static analysis (clang-tidy)
   - Memory safety (AddressSanitizer)
3. Require code review for all changes
4. Run static analyzers regularly

---

## Migration Plan

To bring codebase into compliance:

1. **Phase 1 (Critical - Week 1):**
   - Fix duplicate include (CRIT-001)
   - Standardize error handling (CRIT-003)
   - Implement ConnectionContext (CRIT-002)

2. **Phase 2 (High Priority - Weeks 2-4):**
   - Convert lock_manager to RAII
   - Implement logging framework
   - Move magic numbers to config
   - Fix raw pointer safety issues

3. **Phase 3 (Medium Priority - Weeks 5-8):**
   - Standardize naming with clang-format
   - Add const correctness
   - Remove commented code
   - Standardize comments

4. **Phase 4 (Documentation - Weeks 9-12):**
   - Add Doxygen to all public APIs
   - Generate documentation
   - Create architecture diagrams
   - Update all specifications

**Estimated Total Time:** 3 months for full compliance

---

**Note:** This document reflects the ACTUAL state of the codebase as of October 6, 2025, based on comprehensive code audit. Inconsistencies documented here are known issues being tracked for resolution.

**See Also:**
- [Code Audit Report](../audits/audit_2025_10_06.md) - Detailed findings
- [TODO.md](TODO.md) - Prioritized work items
- [Current Status](../status/CURRENT_STATUS.md) - Implementation status
