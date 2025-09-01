# Phase 1: Core Entry Point and Version

## Objective
Implement minimal working executable with version reporting.

## Prerequisites
- Build system configured
- Directory structure created

## Tasks

### 1.1 Main Entry Point
- Create `src/main.cpp`
- Parse command-line arguments
- Display version when requested
- Return appropriate exit codes

### 1.2 Version Management
- Implement `scratchbird::version()` in `src/scratchbird.cpp`
- Define version constants in `include/scratchbird.h`
- Support semantic versioning (MAJOR.MINOR.PATCH)

### 1.3 Basic Error Handling
- Define `StatusCode` enum
- Implement `Status` structure
- Create error reporting mechanism

## Files to Create/Modify
- `src/main.cpp`
- `src/scratchbird.cpp`
- `include/scratchbird.h`

## Validation Tests
```cpp
// Test version string format
assert(scratchbird::version() matches "X.Y.Z");

// Test command-line parsing
assert(./scratchbird --version returns 0);
assert(./scratchbird --help returns 0);
assert(./scratchbird --invalid returns 1);
```

## Exit Criteria
- Executable builds and runs
- Version string displayed correctly
- Exit codes match conventions (0=success, non-zero=error)