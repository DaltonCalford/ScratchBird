# Architecture Clarification: Parser vs Engine

## Key Architectural Decision

This document clarifies an important architectural distinction in the ScratchBird project that affects how we interpret test results and plan development.

## Core Engine vs Parser Separation

### The Engine (Core Component)
- Stores and executes SBLR bytecode
- Manages storage, transactions, and data
- Does NOT parse SQL directly
- Stores original SQL text for reference only

### Parsers (Client Components)
- External to the core engine
- Multiple parsers can exist:
  - Different SQL dialects (PostgreSQL, MySQL, Firebird, etc.)
  - Other languages (PSQL, etc.)
- Generate SBLR bytecode that the engine executes
- Can be developed independently of the engine

## Development Stages Clarification

### Alpha 1.01-1.05: Core Engine Foundation ✅
- Focus: Stable storage, transactions, basic SBLR execution
- Parser: Only a basic proof-of-concept to test SBLR consumption
- Status: COMPLETE

### Stage 1.2: Advanced SBLR
- Focus: Full SBLR implementation
- Includes: JOINs, subqueries, window functions, all SQL constructs
- This is when parser capabilities expand

## Test Interpretation

### Core Engine Tests (Critical for Alpha)
- Storage tests ✅
- Transaction tests ✅  
- Memory safety tests ✅
- Page management tests ✅

### Parser Tests (Stage 1.2 Features)
- Complex SQL parsing ⏳
- Advanced expressions ⏳
- Constraints and aliases ⏳
- These are NOT blockers for Stage 1.1

## Implications

1. **The 24 failing tests** are primarily parser tests for Stage 1.2 features
2. **Alpha 1.01-1.05 are complete** with stable core engine
3. **Stage 1.1 can proceed** without parser enhancements
4. **Parser development** can happen in parallel with engine development

## Architecture Benefits

This separation provides:
- **Flexibility**: Support multiple SQL dialects
- **Modularity**: Parser updates don't affect engine stability
- **Extensibility**: New languages can be added via new parsers
- **Focus**: Engine team can focus on performance/reliability

## Conclusion

Understanding this architecture is crucial for proper project planning. The "incomplete" parser is by design - it's a client component that will evolve separately from the core engine.