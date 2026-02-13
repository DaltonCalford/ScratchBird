# Wave 3: Agent Pre-Approval Configuration

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Purpose**: Pre-approve common operations for Wave 3 agents to enable autonomous work
**Location**: `/home/dcalford/CliWork/ScratchBird/.claude/settings.local.json`

---

## Recommended Permissions for Wave 3 Agents

Add these to the `"allow"` array in `settings.local.json`:

```json
{
  "permissions": {
    "allow": [
      // ============ WAVE 3: R-TREE AGENTS (R1-R4) ============
      // File creation and editing for R-tree implementation
      "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/core/rtree.h)",
      "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/core/rtree_node.h)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/core/rtree.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/core/rtree_node.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_rtree.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/include/scratchbird/core/index.h)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/optimizer/query_planner.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/optimizer/cost_model.cpp)",

      // ============ WAVE 3: GEOS AGENTS (G1-G4) ============
      // File creation and editing for GEOS integration
      "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/geo/geos_wrapper.h)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/geo/geos_wrapper.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_spatial_functions.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/sblr/bytecode_generator.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/opcodes.h)",

      // ============ WAVE 3: MULTI-GEOMETRY AGENT (M1) ============
      // File creation and editing for multi-geometry types
      "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/geo/multi_geometry.h)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/geo/multi_geometry.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_multi_geometry.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/geo/geometry.cpp)",

      // ============ WAVE 3: PROJ AGENTS (S1-S3) ============
      // File creation and editing for PROJ integration
      "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/geo/srid.h)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/geo/srid.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/geo/proj_wrapper.h)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/geo/proj_wrapper.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_srid.cpp)",

      // ============ COMPILATION AND TESTING ============
      // Build system (CMake + Make)
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && cmake ..)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && make -j$(nproc))",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && make scratchbird_core)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && make scratchbird_tests)",

      // Test execution
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ctest --output-on-failure)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ctest -R test_rtree)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ctest -R test_spatial)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ctest -R test_multi_geometry)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ctest -R test_srid)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ./test_rtree)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ./test_spatial_functions)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ./test_multi_geometry)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ./test_srid)",

      // Direct compilation for quick iteration
      "Bash(g++ -std=c++20 -I include -c src/core/rtree.cpp -o /tmp/rtree.o)",
      "Bash(g++ -std=c++20 -I include -c src/geo/geos_wrapper.cpp -o /tmp/geos_wrapper.o -lgeos_c)",
      "Bash(g++ -std=c++20 -I include -c src/geo/proj_wrapper.cpp -o /tmp/proj_wrapper.o -lproj)",

      // ============ CODE QUALITY ============
      // Syntax checking
      "Bash(g++ -std=c++20 -I include -fsyntax-only src/core/rtree.cpp)",
      "Bash(g++ -std=c++20 -I include -fsyntax-only src/geo/*.cpp)",
      "Bash(clang-format --dry-run --Werror src/core/rtree.cpp)",
      "Bash(clang-format --dry-run --Werror src/geo/*.cpp)",

      // Static analysis (if available)
      "Bash(clang-tidy src/core/rtree.cpp -- -I include -std=c++20)",
      "Bash(cppcheck --enable=all --inconclusive --std=c++20 src/core/rtree.cpp)",

      // ============ DEBUGGING AND MONITORING ============
      // Check compilation errors
      "Bash(grep -r \"error:\" /home/dcalford/CliWork/ScratchBird/build/*.log)",
      "Bash(tail -50 /home/dcalford/CliWork/ScratchBird/build/CMakeFiles/CMakeError.log)",

      // Check test results
      "Bash(cat /home/dcalford/CliWork/ScratchBird/build/Testing/Temporary/LastTest.log)",

      // File inspection
      "Bash(wc -l include/scratchbird/core/rtree*.h src/core/rtree*.cpp)",
      "Bash(wc -l include/scratchbird/geo/*.h src/geo/*.cpp)",
      "Bash(find src/core -name \"rtree*\" -type f)",
      "Bash(find src/geo -name \"*.cpp\" -type f)",

      // ============ DOCUMENTATION ============
      // Update documentation
      "Edit(//home/dcalford/CliWork/ScratchBird/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/FEATURE_PARITY_ROADMAP.md)",
      "Edit(//home/dcalford/CliWork/ScratchBird/README.md)",
      "Write(//home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/status/WAVE_3_*.md)",

      // ============ VERSION CONTROL (Optional) ============
      // Git operations for agent-generated code
      "Bash(git status)",
      "Bash(git diff src/core/rtree*)",
      "Bash(git diff src/geo/*)",
      "Bash(git add src/core/rtree* tests/unit/test_rtree.cpp)",
      "Bash(git add src/geo/* tests/unit/test_spatial_functions.cpp)",
      "Bash(git commit -m \"Wave 3 Agent *: *\")"
    ]
  }
}
```

---

## Wildcard Permission Patterns

For maximum flexibility, you can use wildcard patterns:

```json
{
  "permissions": {
    "allow": [
      // Allow all file operations in specific directories
      "Write(//home/dcalford/CliWork/ScratchBird/src/core/rtree*)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/geo/**)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_*.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/**/*.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/include/**/*.h)",

      // Allow all CMake and Make operations
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && *)",
      "Bash(cmake *)",
      "Bash(make *)",

      // Allow all test execution
      "Bash(ctest *)",
      "Bash(./test_*)",

      // Allow all g++ compilation
      "Bash(g++ -std=c++20 *)"
    ]
  }
}
```

---

## Recommended Configuration Levels

### Level 1: Conservative (Manual approval for major changes)
**Use when**: You want to review every significant file change

```json
"allow": [
  "Read(//home/dcalford/CliWork/ScratchBird/**)",
  "Bash(g++ -std=c++20 -fsyntax-only *)",  // Syntax check only
  "Bash(cmake --build build --target help)",
  "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_*.cpp)"  // Allow test creation
]
```

**Agent must ask before**:
- Modifying production code (`src/`, `include/`)
- Running full builds
- Committing to git

---

### Level 2: Balanced (Auto-approve common operations) ⭐ RECOMMENDED
**Use when**: You trust agents to implement within scope

```json
"allow": [
  // File operations for Wave 3 scope
  "Write(//home/dcalford/CliWork/ScratchBird/src/core/rtree*)",
  "Write(//home/dcalford/CliWork/ScratchBird/src/geo/**)",
  "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_*.cpp)",
  "Edit(//home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp)",
  "Edit(//home/dcalford/CliWork/ScratchBird/src/optimizer/*.cpp)",

  // Build and test
  "Bash(cd /home/dcalford/CliWork/ScratchBird/build && cmake ..)",
  "Bash(cd /home/dcalford/CliWork/ScratchBird/build && make *)",
  "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ctest *)",
  "Bash(g++ -std=c++20 *)"
]
```

**Agent must ask before**:
- Modifying files outside Wave 3 scope
- Git operations (commit, push)
- Deleting files

---

### Level 3: Autonomous (Maximum automation)
**Use when**: Running overnight or wanting minimal interruptions

```json
"allow": [
  // Full file system access within project
  "Write(//home/dcalford/CliWork/ScratchBird/src/**)",
  "Write(//home/dcalford/CliWork/ScratchBird/include/**)",
  "Write(//home/dcalford/CliWork/ScratchBird/tests/**)",
  "Edit(//home/dcalford/CliWork/ScratchBird/**/*.cpp)",
  "Edit(//home/dcalford/CliWork/ScratchBird/**/*.h)",

  // Full build system access
  "Bash(cd /home/dcalford/CliWork/ScratchBird/build && *)",
  "Bash(cmake *)",
  "Bash(make *)",
  "Bash(ctest *)",
  "Bash(g++ *)",

  // Version control (local only, no push)
  "Bash(git add *)",
  "Bash(git commit *)",
  "Bash(git status)",
  "Bash(git diff *)"
]
```

**Agent must ask before**:
- `git push` (pushing to remote)
- Destructive operations (`rm -rf`, `git reset --hard`)

---

## Implementation Instructions

### Step 1: Backup Current Configuration

```bash
cp /home/dcalford/CliWork/ScratchBird/.claude/settings.local.json \
   /home/dcalford/CliWork/ScratchBird/.claude/settings.local.json.backup
```

### Step 2: Choose Configuration Level

**Recommended**: Level 2 (Balanced)

### Step 3: Update settings.local.json

Add the permissions from your chosen level to the existing `"allow"` array.

**Example** (Level 2):
```json
{
  "permissions": {
    "allow": [
      // ... existing permissions ...

      // ============ WAVE 3: SPATIAL COMPLETION ============
      "Write(//home/dcalford/CliWork/ScratchBird/src/core/rtree*)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/geo/**)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_*.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/optimizer/*.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/opcodes.h)",

      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && cmake ..)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && make *)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ctest *)",
      "Bash(g++ -std=c++20 *)",
      "Bash(./test_*)"
    ]
  }
}
```

### Step 4: Verify Configuration

```bash
# Check syntax
python3 -m json.tool /home/dcalford/CliWork/ScratchBird/.claude/settings.local.json > /dev/null && echo "✓ Valid JSON"

# Check permissions loaded
grep -A 5 "WAVE 3" /home/dcalford/CliWork/ScratchBird/.claude/settings.local.json
```

---

## Agent-Specific Permissions

You can also scope permissions per agent:

### Agent R1 (R-tree Core)
```json
"allow": [
  "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/core/rtree.h)",
  "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/core/rtree_node.h)",
  "Write(//home/dcalford/CliWork/ScratchBird/src/core/rtree_node.cpp)",
  "Bash(g++ -std=c++20 -I include -c src/core/rtree_node.cpp)"
]
```

### Agent G1 (GEOS Wrapper)
```json
"allow": [
  "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/geo/geos_wrapper.h)",
  "Write(//home/dcalford/CliWork/ScratchBird/src/geo/geos_wrapper.cpp)",
  "Bash(g++ -std=c++20 -I include -c src/geo/geos_wrapper.cpp -lgeos_c)"
]
```

### Agent M1 (Multi-Geometry)
```json
"allow": [
  "Write(//home/dcalford/CliWork/ScratchBird/include/scratchbird/geo/multi_geometry.h)",
  "Write(//home/dcalford/CliWork/ScratchBird/src/geo/multi_geometry.cpp)",
  "Edit(//home/dcalford/CliWork/ScratchBird/src/geo/geometry.cpp)"
]
```

---

## Safety Features

### 1. Deny List (Override allows)
```json
"deny": [
  "Bash(rm -rf *)",
  "Bash(git push --force *)",
  "Bash(sudo *)",
  "Bash(chmod 777 *)",
  "Write(//home/dcalford/CliWork/ScratchBird/.git/**)"
]
```

### 2. Always Ask (Even if allowed)
```json
"ask": [
  "Bash(git push *)",
  "Edit(//home/dcalford/CliWork/ScratchBird/CMakeLists.txt)",
  "Edit(//home/dcalford/CliWork/ScratchBird/README.md)"
]
```

---

## Benefits of Pre-Approval

### For Wave 3 Development

**Without Pre-Approval**:
- Agent generates code → stops → asks for Write permission
- You approve → agent writes file → stops → asks for Bash(g++)
- You approve → agent compiles → stops → asks for Bash(./test)
- **Result**: 10-20 approvals per agent, constant interruptions

**With Pre-Approval** (Level 2):
- Agent generates code → writes file (auto-approved)
- Agent compiles → runs (auto-approved)
- Agent tests → reports results (auto-approved)
- Agent only asks for: out-of-scope changes, destructive operations
- **Result**: 0-2 approvals per agent, autonomous workflow

### Time Savings

**Per Agent**:
- Conservative: ~2-3 hours of developer time for approvals
- Balanced: ~15-30 minutes for occasional approvals
- Autonomous: ~5-10 minutes for rare approvals

**All 12 Agents**:
- Conservative: 24-36 hours developer time
- Balanced: 3-6 hours developer time ⭐
- Autonomous: 1-2 hours developer time

---

## Monitoring Agent Activity

Even with pre-approval, you can monitor what agents do:

### 1. Git Tracking
```bash
# See all changes made by agents
git log --author="Claude" --oneline

# Review specific agent's work
git log --author="Agent R1" --patch
```

### 2. Build Logs
```bash
# Monitor compilation
tail -f /home/dcalford/CliWork/ScratchBird/build/compile.log

# Check test results
watch -n 5 'cd /home/dcalford/CliWork/ScratchBird/build && ctest --output-on-failure'
```

### 3. File System Monitoring
```bash
# Watch for new files
watch -d 'find /home/dcalford/CliWork/ScratchBird/src/core -name "rtree*" -ls'
```

---

## Rollback Strategy

If an agent makes unwanted changes:

```bash
# Rollback last commit
git reset --soft HEAD~1

# Discard all uncommitted changes
git checkout -- .

# Restore specific file
git checkout HEAD -- src/core/rtree.cpp

# Restore from backup
cp /home/dcalford/CliWork/ScratchBird/.claude/settings.local.json.backup \
   /home/dcalford/CliWork/ScratchBird/.claude/settings.local.json
```

---

## Recommended Configuration for Wave 3

```json
{
  "permissions": {
    "allow": [
      // ... existing permissions remain ...

      // ============ WAVE 3: FILE OPERATIONS ============
      "Write(//home/dcalford/CliWork/ScratchBird/src/core/rtree*)",
      "Write(//home/dcalford/CliWork/ScratchBird/src/geo/**)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_rtree.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_spatial_functions.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_multi_geometry.cpp)",
      "Write(//home/dcalford/CliWork/ScratchBird/tests/unit/test_srid.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/sblr/bytecode_generator.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/opcodes.h)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/optimizer/query_planner.cpp)",
      "Edit(//home/dcalford/CliWork/ScratchBird/src/optimizer/cost_model.cpp)",

      // ============ WAVE 3: BUILD & TEST ============
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && cmake ..)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && make *)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ctest *)",
      "Bash(cd /home/dcalford/CliWork/ScratchBird/build && ./test_*)",
      "Bash(g++ -std=c++20 *)",

      // ============ WAVE 3: CODE QUALITY ============
      "Bash(clang-format *)",
      "Bash(clang-tidy *)",
      "Bash(wc -l *)"
    ],
    "deny": [
      "Bash(rm -rf *)",
      "Bash(git push --force *)",
      "Bash(sudo *)"
    ],
    "ask": [
      "Bash(git push *)",
      "Edit(//home/dcalford/CliWork/ScratchBird/CMakeLists.txt)"
    ]
  }
}
```

---

## Next Steps

1. **Backup current configuration**:
   ```bash
   cp .claude/settings.local.json .claude/settings.local.json.backup
   ```

2. **Choose Level 2 (Balanced)** - Recommended for Wave 3

3. **Update `.claude/settings.local.json`** with Wave 3 permissions

4. **Verify**:
   ```bash
   python3 -m json.tool .claude/settings.local.json > /dev/null
   ```

5. **Launch Wave 3 agents** - they will now work autonomously

---

**Status**: Ready to configure
**Recommended Level**: Level 2 (Balanced) - 85% autonomous, minimal interruptions
**Expected Approval Count**: 0-2 per agent (vs. 10-20 without pre-approval)
