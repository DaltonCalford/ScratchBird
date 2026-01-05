# SHOW/SET Correction Plan Checklist (Actual)

Purpose: Focused checklist of SHOW/SET mismatches between specs, V2 parser/bytecode, and executor behavior.

Status: static code review snapshot; no runtime execution performed.

Scope and sources:
- `ScratchBird-Analysis/reports/ai_docs/25_show_set_commands_actual.md`
- `ScratchBird/src/parser/parser_v2.cpp`
- `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- `ScratchBird/src/sblr/executor.cpp`
- `ScratchBird/include/scratchbird/parser/ast_v2.h`
- `ScratchBird/include/scratchbird/sblr/opcodes.h`
- `ScratchBird/docs/specifications/00_GRAMMAR_BNF.md`
- `ScratchBird/docs/specifications/07_TRANSACTION_AND_SESSION_CONTROL.md`
- `ScratchBird/docs/specifications/ScratchBird Master Grammar Specification v2.0.md`

## Checklist: Pipeline breakages (must-fix)
- [ ] Add executor handlers for EXT_SHOW_VARIABLE, EXT_SHOW_ALL, EXT_SHOW_TRANSACTION_LEVEL or stop emitting them from V2.
- [ ] Fix SET ROLE payload: generator should emit flags byte (bit0=reset) + role string; executor currently expects flags.
- [ ] Fix SET SESSION AUTHORIZATION payload: generator should emit flags byte (bit0=reset) + user string; executor expects flags.
- [ ] Add bytecode emission for SET TIME ZONE and executor storage (no opcode today; decide on EXT_SET_TIME_ZONE or explicit variable semantics).
- [ ] Implement SET CONSTRAINTS parsing and bytecode emission (EXT_SET_CONSTRAINTS exists and executor implements it).

## Checklist: Parser and grammar alignment
- [ ] Decide canonical SET SCHEMA syntax (BNF vs Master Grammar) and accept aliases (SET CURRENT SCHEMA, SET SCHEMA TO/=).
- [ ] Decide canonical SET SEARCH_PATH syntax (SEARCH_PATH vs SEARCH PATH) and accept aliases.
- [ ] Implement SET SESSION CHARACTERISTICS AS TRANSACTION as alias of SET TRANSACTION or remove from spec.
- [ ] Implement DESCRIBE/DESC (alias for SHOW COLUMNS) or remove from grammar.
- [ ] Require names where executor requires them (SHOW TRIGGER/VIEW/PROCEDURE/FUNCTION/DOMAIN/GENERATOR/ROLE/CHECKS).

## Checklist: Coverage gaps vs specs
- [ ] Implement SHOW SEARCH_PATH, SHOW TIME ZONE, SHOW ALL (session-control spec) using schema-navigation opcodes or variable show.
- [ ] Decide support for SHOW VARIABLES/STATUS/WARNINGS/ERRORS/PROCESSLIST and SHOW CREATE DATABASE (BNF).
- [ ] Decide support for MySQL user variables (SET @var, SELECT INTO @var) or remove from BNF.
- [ ] Decide support for SHOW SCHEMAS and map to SHOW DATABASES (list schemas) or implement separate.

## Checklist: Bytecode and data model alignment
- [ ] Preserve SET SEARCH_PATH list values in semantic analyzer and emit BEGIN_LIST in bytecode; executor already supports list form.
- [ ] Encode SESSION/LOCAL scope in bytecode or remove scope from grammar if it will remain no-op.
- [ ] Define payloads for schema-navigation SHOW commands (SCHEMA PATH/TREE/SEARCH PATH/LOCATION/RESOLVED/OBJECTS) and add parser emission.
- [ ] Clarify behavior for SHOW INDEXES with no FROM (error vs list all) and align parser/executor accordingly.
- [ ] Decide whether generic SET should accept more variables beyond SEARCH_PATH or restrict grammar to SEARCH_PATH only.
