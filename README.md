# ScratchBird Database Engine

## Status: Alpha Phase 1.05 Complete

ScratchBird is an experimental project developing a complex database engine. Early foundation work is complete and next stage planning/implementation is currently going on.

### Completed Phases

- **Alpha 1.01** - Database Core
- **Alpha 1.02** - System Catalog
- **Alpha 1.03** - Storage Engine
- **Alpha 1.04** - Transaction Foundation
- **Alpha 1.05** - Basic SQL Parser For Initial Testing

### Next Phase

- **Alpha Stage 1.1** - Extended Storage (64K/128K), Compression, TOAST/LOB — planning

## Project Structure

- **`ProjectPlan/`** - Complete project specifications and phase breakdown
  - Phase specifications (what needs to be built)
  - Architecture documents
  - Progress tracking logs
  
- **`docs/`** - Architecture and design documentation
  - Architecture decision records
  - Compatibility specifications
  - Feature specifications

- **`references/`** - External reference materials and specifications
  
- **`src/`** - Source code
  - `core/` - Database engine
  - `parser/` - SQL parser
  - `sblr/` - Bytecode system

- **`tests/`** - Test suites
  - `unit/` - Unit tests
  - `integration/` - Integration tests

## Development Process

1. Review `AUTHORITATIVE_IMPLEMENTATION_PLAN.md`
2. Begin implementation of next Alpha phase
3. Progress tracked in `ProjectPlan/progress/` logs
4. Tests created alongside implementation
5. Coding involves four elements
   1. Project Reviewer : Monitors commits to prevent veering from plan
   2. Code Implementor (agent A) : Reads the specifications, asks for clarification if needed, writes the code. Implements any changes agent B requires and ensures that tests agent C produces are working or that there is a DEV accepted reason for them not working.
   3. Code Reviewer (agent B) : Reads the specifications, reads the decision documents in the doc directory. Checks for memory leaks, security issues, possible buffer overruns and other base checks. Checks for maintaining the coding standards as per the specification. Performs a deep analysis of the code including possibly building and testing the code to ensure it meets the specifications. agent B does not alter the code in any way - it ensures that Agent A is not missing anything or stating falsehoods.
   4. Test Writer (agent C): While Agent A will create basic tests to ensure their code is correct, Agent C creates performance/hardening/edge case tests and they are the core tests that A must ensure are running or have a reason for their not running (such as the tests asking for functionality being added in later phases)

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Testing

```bash
# Run all tests
ctest --output-on-failure

# Run specific tests
ctest -R "Alpha101"
```

## License

See LICENSE file for details.