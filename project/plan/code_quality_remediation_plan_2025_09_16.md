# Code Quality Remediation Plan

**Date:** 2025-09-16
**Author:** Agent B (Code Reviewer)
**Source:** `project/reviews/agent_b_codebase_analysis_2025_09_16.md`

---

## PLAN STATUS: PARTIALLY COMPLETE - PHASES 2-3 NOT EXECUTED

**Execution Status:**
- ✅ Phase 1.1: `.clang-format` created
- ✅ Phase 1.2: `.clang-tidy` created
- ✅ Phase 1.3: CMakeLists.txt integration complete
- ❌ Phase 2.1: Automated formatting NOT applied to codebase
- ❌ Phase 2.2: Naming convention fixes NOT applied
- ❌ Phase 2.3: Manual remediation NOT done
- ❌ Phase 3.1: Build verification FAILED (20 compilation errors)
- ❌ Phase 3.2: Tests NOT run (build broken)

**Current Impact:**
The build is broken with 20 compilation errors. Many errors are due to naming inconsistencies (snake_case vs camelCase) that Phase 2 was designed to fix but was never executed.

**Critical Issues:**
- `should_compress_page` and `is_compressible_page` - undeclared identifiers
- These are the exact naming issues this plan was meant to address

**This plan completion is P0 CRITICAL for unblocking all development work.**

---

**Objective:** This document outlines the critical, sequential tasks required to remediate the systemic code quality issues in the ScratchBird codebase. All feature development, including the column UUID migration, is to be halted until this plan is completed and verified.

---

## Phase 1: Automated Tooling Integration

This phase focuses on establishing a foundation of automated quality control by integrating standard C++ tooling into the project.

### 1.1: Create `.clang-format` Configuration
- **Task:** Create a `.clang-format` file in the root directory of the project.
- **Details:**
    - [ ] The configuration within this file must strictly adhere to the formatting rules defined in `docs/development/CODING_STANDARDS.md` (e.g., 4-space indentation, Unix line endings).
    - [ ] The style should be based on a standard format like "LLVM" or "Google" and then customized to match the project's specific standards.

### 1.2: Create `.clang-tidy` Configuration
- **Task:** Create a `.clang-tidy` file in the root directory of the project.
- **Details:**
    - [ ] The configuration must enable checks that enforce the project's coding standards.
    - [ ] Key checks to include are:
        - Naming conventions (e.g., `readability-identifier-naming`) to enforce `camelCase` for functions and `snake_case` for variables.
        - Use of modern C++ features (e.g., `modernize-use-enum-class`, `modernize-use-constexpr`).
        - Performance and best-practice checks.

### 1.3: Integrate Tooling into the Build Process
- **Task:** Modify the root `CMakeLists.txt` file to automatically run `clang-tidy` during the build process and to provide a formatting target.
- **Details:**
    - [ ] Use `find_program` to locate `clang-tidy`.
    - [ ] Add the `CMAKE_CXX_CLANG_TIDY` variable to the CMake configuration, pointing it to the `clang-tidy` executable and its configuration file. This will run the linter on every build and report warnings.
    - [ ] Add a new custom target (e.g., `format`) that developers can run to automatically format the entire codebase using `clang-format`.

---

## Phase 2: Codebase-Wide Remediation Pass

This phase focuses on bringing the existing code into compliance with the newly enforced standards.

### 2.1: Automated Code Formatting
- **Task:** Reformat the entire codebase using the new `.clang-format` configuration.
- **Details:**
    - [ ] Run the newly created `format` target from the build directory.
    - [ ] Commit the resulting formatting changes as a single, dedicated commit with the message "Style: Apply clang-format to entire codebase."

### 2.2: Automated Naming Convention Fixes
- **Task:** Use `clang-tidy`'s automated fixing capabilities to correct as many naming convention violations as possible.
- **Details:**
    - [ ] Run `clang-tidy` with the `-fix` flag on all source files.
    - [ ] This should automatically rename many of the `snake_case` functions to `camelCase`.
    - [ ] Review and commit the automated changes with the message "Refactor: Apply automated clang-tidy naming fixes."

### 2.3: Manual Remediation of Remaining Issues
- **Task:** Manually address all remaining violations that could not be fixed automatically.
- **Details:**
    - [ ] **Constants:** Manually refactor all C-style `#define` macros used for constants into `enum class` or `constexpr` variables, as appropriate. This is the highest priority manual task.
    - [ ] **API Consistency:** Manually standardize all status checks to a single, consistent method (e.g., `== Status::Ok`).
    - [ ] **Remaining Linter Warnings:** Manually fix any other warnings reported by `clang-tidy` that require developer intervention.

---

## Phase 3: Verification

This phase ensures that the remediation was successful and the codebase is in a stable, high-quality state.

### 3.1: Compile the Project
- **Task:** Build the entire ScratchBird project from a clean build directory.
- **Details:**
    - [ ] Run `cmake ..` and `make` from the `build` directory.
    - [ ] The build must complete without any errors or new warnings from `clang-tidy`.

### 3.2: Run All Tests
- **Task:** Execute all unit and integration tests.
- **Details:**
    - [ ] Run `ctest --output-on-failure` from the `build` directory.
    - [ ] All existing tests must pass.

---

Once this remediation plan is complete, the original task of migrating columns to UUIDs can be safely resumed.
