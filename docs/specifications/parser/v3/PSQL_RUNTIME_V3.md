# V3 PSQL Runtime Semantics
Status: Authoritative (V3)
Last Updated: 2026-02-08

This document defines runtime semantics for V3 PSQL execution, including
variable scoping, exception propagation, cursor lifecycle, and loop behavior.

## 1. Execution Model
- PSQL executes within a **routine frame** (procedure/function).
- Each routine frame contains a **value stack**, **local variables**, and
  **exception handlers**.
- PSQL opcodes are executed by the SBLR VM described in `EXECUTOR_V3_SBLR.md`.

## 2. Variable Scoping (Normative)
### 2.1 Scope Types
- **Routine scope**: parameters and top-level locals.
- **Block scope**: locals declared within `BLOCK`.
- **Loop scope**: implicit scope for loop control variables.
- **Handler scope**: locals declared within exception handler blocks.

### 2.2 Resolution Order
Variable lookup follows:
1. Innermost block/handler scope.
2. Enclosing block scopes.
3. Routine scope (parameters and locals).
4. Session-level variables (if explicitly referenced).

### 2.3 Declaration Rules
- `DECLARE` and `PARAM_*` must appear before executable statements in the same
  block.
- Names must be unique within a scope; shadowing is allowed but must be explicit.
- Domain-typed variables enforce domain validation on assignment.

### 2.4 Lifetime Rules
- Block locals are allocated at block entry and released on block exit.
- Loop variables are allocated at loop entry and released on loop exit.
- Handler locals are allocated at handler entry and released at handler exit.

## 3. Assignment and Type Semantics
- `ASSIGN` and `VAR_STORE` perform:
  1. Type coercion (if needed and allowed).
  2. Domain validation (NOT NULL, CHECK, range).
  3. Normalization (domain-specific canonicalization).
- NULL assignment to NOT NULL variables raises a runtime exception.

## 4. Exception Propagation (Normative)
### 4.1 Exception Types
- **Runtime exceptions**: type mismatch, null violations, constraint failures.
- **User-defined exceptions**: raised via `RAISE`.

### 4.2 Propagation Rules
1. When an exception occurs, unwind the stack to the nearest active handler.
2. If a matching handler exists, execute handler block.
3. If handler re-raises, continue unwinding to next enclosing handler.
4. If no handler exists, abort routine and propagate to caller.

### 4.3 Handler Matching
Handlers match by:
1. SQLSTATE (exact match).
2. Exception name.
3. Default handler (`WHEN OTHERS`).

### 4.4 Savepoint Interaction
- Each PSQL statement implicitly creates a sub-savepoint.
- On exception, rollback to the last sub-savepoint before handler execution.

## 5. Cursor Lifecycle (Normative)
### 5.1 States
Cursor states: `DECLARED -> OPEN -> FETCHING -> CLOSED`.

### 5.2 Rules
- `CURSOR_DECLARE` allocates cursor metadata but does not execute the query.
- `CURSOR_OPEN` binds parameters and initializes the query plan.
- `CURSOR_FETCH` advances the cursor; EOF returns false/NULL as defined by payload.
- `CURSOR_CLOSE` releases resources and invalidates cursor handle.

### 5.3 Error Conditions
- Fetch on unopened cursor → runtime error.
- Open on already-open cursor → runtime error.
- Close on already-closed cursor → no-op unless strict mode enabled.

## 6. Loop Semantics (Normative)
### 6.1 WHILE / LOOP
- `WHILE` evaluates condition each iteration; NULL treated as false.
- `LOOP` is infinite unless exited explicitly.

### 6.2 FOR SELECT
- `PSQL_FOR_SELECT` opens an implicit cursor, iterates rows, and assigns row
  values to target variables.
- Cursor is closed automatically on loop exit.

### 6.3 FOR EXECUTE
- `PSQL_FOR_EXECUTE` re-prepares the statement if dynamic SQL changes.
- Each iteration rebinds parameters and executes the statement.

### 6.4 EXIT / CONTINUE
- `PSQL_LEAVE` exits the innermost (or labeled) loop.
- `PSQL_CONTINUE` continues to the next iteration.
- Exiting a loop releases loop-scope variables.

## 7. Control Flow Semantics
- `IF/ELSIF/ELSE` follow three-valued logic (NULL treated as false unless
  payload specifies UNKNOWN handling).
- `JUMP` and `LABEL` use resolved offsets; jumping into a deeper scope is invalid.

## 8. Determinism and Side Effects
- Side-effecting expressions inside PSQL must be evaluated exactly once per
  execution path.
- Sub-statements invoked by PSQL must obey transaction scope rules.

## 9. Validation Checklist
- All declarations occur before executable statements in a block.
- Variable resolution matches scope rules.
- Exception handlers are properly nested and match by SQLSTATE/name.
- Cursor operations follow valid lifecycle order.
- Loop exits correctly release scope variables.
