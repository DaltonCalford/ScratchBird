# Query Processing Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains query optimization and execution specifications for ScratchBird's query processor.

## Overview

ScratchBird implements a cost-based query optimizer that generates optimal execution plans for SQL queries across multiple dialects. The optimizer integrates with the MGA transaction system and supports advanced features like predicate pushdown, join reordering, and index selection.

## Specifications in this Directory

- **[QUERY_OPTIMIZER_SPEC.md](QUERY_OPTIMIZER_SPEC.md)** (1,248 lines) - Comprehensive query optimizer specification
- **[PARALLEL_EXECUTION_ARCHITECTURE.md](PARALLEL_EXECUTION_ARCHITECTURE.md)** - Parallel execution architecture (Beta)

## Key Features

### Cost-Based Optimization

The query optimizer uses cost models for:

- **Index selection** - Choose optimal index for query predicates
- **Join ordering** - Determine most efficient join order
- **Scan methods** - Sequential scan vs. index scan vs. bitmap scan
- **Parallelism** - Parallel query execution planning

See **PARALLEL_EXECUTION_ARCHITECTURE.md** for the execution model and cluster extension details.

### Query Transformations

Supported query transformations:

- **Predicate pushdown** - Push predicates to lowest possible level
- **Projection pushdown** - Eliminate unnecessary columns early
- **Join elimination** - Remove unnecessary joins
- **Subquery flattening** - Convert correlated subqueries to joins
- **Common subexpression elimination** - Eliminate duplicate computations

### MGA Integration

The optimizer understands MGA transaction semantics:

- **Snapshot visibility** - Plan respects transaction snapshot
- **Version selection** - Efficient filtering of record versions
- **Garbage collection hints** - Signal opportunities for GC

## Query Execution Pipeline

1. **Parsing** - SQL text → AST (see [Parser](../parser/))
2. **Semantic Analysis** - Validate and resolve identifiers
3. **Optimization** - AST → Optimized logical plan
4. **Bytecode Generation** - Logical plan → SBLR bytecode (see [SBLR](../sblr/))
5. **Execution** - SBLR interpreter executes query

## Related Specifications

- [Parser](../parser/) - SQL parsing and AST generation
- [SBLR Bytecode](../sblr/) - Query execution bytecode
- [Indexes](../indexes/) - Index structures and access methods
- [Transaction System](../transaction/) - Snapshot visibility and MGA
- [Storage Engine](../storage/) - Page access and buffer management

## Critical Reading

Before working on query optimization:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ:** [QUERY_OPTIMIZER_SPEC.md](QUERY_OPTIMIZER_SPEC.md) - Query optimizer specification

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
