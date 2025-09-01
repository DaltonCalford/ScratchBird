# Phase Implementation Template

## AI IMPLEMENTATION CHECKLIST

### Before Starting ANY Phase

#### Step 1: Confirm Standards Review
```
I have read and will follow:
- [ ] /workspace/references/CODING_AND_BUILD_STANDARDS.md
- [ ] /workspace/ProjectPlan/00_BUILD_AND_STRUCTURE.md
- [ ] /workspace/references/archive/technical_specifications/PAGE_LAYOUTS_AND_STRUCTURES.md (if relevant)
```

#### Step 2: Confirm Build System Usage
```
I will:
- [ ] Use CMake for all builds (no direct g++ compilation)
- [ ] Run tests through ctest (not standalone programs)
- [ ] Follow the project directory structure
- [ ] Use proper naming conventions (lowercase_with_underscores)
```

#### Step 3: Confirm Testing Approach
```
I will test:
- [ ] Alpha: 3 page sizes (8K, 16K, 32K). 64K/128K deferred to Beta.
- [ ] Using Google Test framework
- [ ] Through the CMake/CTest system
- [ ] With proper test organization (tests/phase_X_XX/)
```

### Implementation Workflow

#### 1. Setup Build Environment
```bash
# ALWAYS start with a clean build
cd /workspace
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

#### 2. Create Implementation Files
```bash
# Example for Phase 1.03 - Heap Storage

# Create header (if needed)
touch /workspace/include/scratchbird/engine/storage/heap_manager.h

# Create implementation
touch /workspace/src/engine/storage/heap_manager.cpp

# Create test
mkdir -p /workspace/tests/phase_1_03
touch /workspace/tests/phase_1_03/test_heap_manager.cpp
```

#### 3. Update CMakeLists.txt
```cmake
# In /workspace/src/CMakeLists.txt
# Add to appropriate section, don't create new CMakeLists

# DON'T create random CMakeLists.txt files
# DON'T use add_executable for tests outside of tests/
# DON'T forget to link libraries
```

#### 4. Implement Using Modern C++
```cpp
// CORRECT - Modern C++17
#pragma once
#include <memory>
#include <optional>
#include <variant>

namespace scratchbird::engine {

class HeapManager {
    std::unique_ptr<Page> allocatePage();
    std::optional<PageId> findFreePage();
};

} // namespace scratchbird::engine
```

```cpp
// WRONG - Old style
#ifndef HEAP_MANAGER_H
#define HEAP_MANAGER_H

class HeapManager {
    Page* allocatePage();  // Raw pointer - WRONG
    int error_code;        // Error codes - WRONG
};
```

#### 5. Build and Test
```bash
# From build directory
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Check specific test
ctest -R HeapManager -V
```

#### 6. Verify No Memory Leaks
```bash
# Option 1: Use AddressSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
make -j$(nproc)
ctest

# Option 2: Use Valgrind
valgrind --leak-check=full ./tests/test_heap_manager
```

#### 7. Log Progress
```bash
# Add to implementation log
echo "$(date '+%Y-%m-%d %H:%M') 1.03.1 CREATED heap_manager.h" >> /workspace/ProjectPlan/progress/implementation.log
echo "$(date '+%Y-%m-%d %H:%M') 1.03.2 IMPLEMENTED HeapManager::allocatePage()" >> /workspace/ProjectPlan/progress/implementation.log
echo "$(date '+%Y-%m-%d %H:%M') 1.03.3 TESTED heap allocation for all page sizes" >> /workspace/ProjectPlan/progress/test.log
```

### Common Mistakes to Avoid

#### ❌ NEVER: Create Standalone Programs
```cpp
// WRONG - Don't create random test programs
int main() {
    HeapManager hm;
    hm.test();
    return 0;
}
```

#### ❌ NEVER: Compile Directly
```bash
# WRONG - Don't bypass CMake
g++ -o test test.cpp
./test
```

#### ❌ NEVER: Forget Namespaces
```cpp
// WRONG - Global namespace
class Page { };

// CORRECT - Proper namespace
namespace scratchbird::engine {
class Page { };
}
```

#### ❌ NEVER: Use Raw Memory Management
```cpp
// WRONG
Page* page = new Page();
delete page;

// CORRECT
auto page = std::make_unique<Page>();
```

#### ❌ NEVER: Skip Page Size Testing
```cpp
// WRONG - Testing only one size
TEST(HeapTest, AllocatePage) {
    HeapManager hm(8192);  // Only 8K
    // ...
}

// CORRECT - Test all sizes
TEST(HeapTest, AllocatePageAllAlphaSizes) {
    for (auto size : {8192, 16384, 32768}) {
        HeapManager hm(size);
        // ...
    }
}
```

### Commit Message Template
```
Implement Phase X.XX: [Brief description]

- [Specific feature implemented]
- [Test coverage details]
- Supports all 5 page sizes
- Tests pass with ctest
- No memory leaks (verified with ASAN/Valgrind)

Technical specs referenced:
- references/technical_specifications/[relevant_spec].md

Progress logged:
- implementation.log: X entries
- test.log: Y entries
```

### Final Checklist Before Commit

- [ ] Code builds with `cmake && make`
- [ ] All tests pass with `ctest`
- [ ] No compiler warnings
- [ ] No memory leaks
- [ ] Follows naming conventions
- [ ] Uses modern C++17 features
- [ ] Tests all 5 page sizes
- [ ] CMakeLists.txt updated properly
- [ ] Progress logged
- [ ] Meaningful commit message

## Remember

1. **Standards First** - Always read CODING_AND_BUILD_STANDARDS.md
2. **CMake Always** - Never compile directly
3. **Test Everything** - Especially all page sizes
4. **Modern C++** - Use C++17 features
5. **Clean Code** - RAII, smart pointers, namespaces

This template ensures consistent, high-quality implementations across all phases!