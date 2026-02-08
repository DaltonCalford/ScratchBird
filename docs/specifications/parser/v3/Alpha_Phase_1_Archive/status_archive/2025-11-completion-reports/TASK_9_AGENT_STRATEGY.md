# Task 9 (Spatial Types) - AI Agent Completion Strategy

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Goal**: Complete SQL integration for spatial types (6-8 hours of work)
**Current Status**: Infrastructure 100% complete, SQL layer pending
**Approach**: Single focused agent vs. breaking into sub-tasks

---

## Option 1: Single Comprehensive Agent (RECOMMENDED)

**Why Best**:
- Spatial SQL integration is tightly coupled (parser → bytecode → executor)
- All work builds on the same infrastructure (WKT/WKB already complete)
- Agent can test incrementally and fix issues
- Avoids coordination overhead between multiple agents

**Estimated Time**: 1-2 hours agent execution (vs 6-8 hours manual)

**Agent Specification**:

```python
Task(
    subagent_type="general-purpose",
    description="Complete spatial types SQL integration",
    prompt="""
# Task: Complete Spatial Types SQL Integration (Phase 2 Task 9.1)

## Context
You are completing Phase 2 Task 9.1 for ScratchBird database engine.

**What's Already Done** (Agent 1 delivered):
- ✅ POINT, LINESTRING, POLYGON types implemented (100%)
- ✅ WKT parser complete (349 lines) - bidirectional conversion
- ✅ WKB serializer complete (356 lines) - binary storage
- ✅ TypedValue integration complete (factory methods, getters)
- ✅ 10 spatial opcodes defined in opcodes.h (0x50-0x59 range)
- ✅ 44/44 tests passing - all infrastructure verified

**What You Need to Do** (6-8 hours of work):
1. Add parser support for spatial constructor functions
2. Add bytecode generation for spatial operations
3. Add executor handlers for spatial opcodes
4. Verify all 44 existing tests still pass
5. Test end-to-end SQL queries

## Your Task: SQL Integration

### 1. Parser Integration (src/parser/parser.cpp)

Add parsing for spatial constructor functions in `parseFunctionCall()`:

**Functions to Parse**:
- `ST_Point(x, y)` → POINT type
- `ST_MakeLine(point1, point2, ...)` → LINESTRING type
- `ST_MakePolygon(linestring)` → POLYGON type
- `ST_AsText(geom)` → WKT string output
- `ST_AsBinary(geom)` → WKB binary output
- `ST_GeometryType(geom)` → type name string
- `ST_IsValid(geom)` → boolean validation

**Pattern to Follow**:
Look at existing function parsing (e.g., `JSON_EXTRACT`, `COALESCE`). Follow the same structure:

```cpp
// In parseFunctionCall() around line 2400
if (function_name == "ST_POINT") {
    // Expect ST_Point(x, y)
    if (!consume(TokenType::LEFT_PAREN, "Expected '(' after ST_Point"))
        return nullptr;

    auto* x_expr = parseExpression();
    if (!x_expr) return nullptr;

    if (!consume(TokenType::COMMA, "Expected ',' in ST_Point"))
        return nullptr;

    auto* y_expr = parseExpression();
    if (!y_expr) return nullptr;

    if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after ST_Point arguments"))
        return nullptr;

    std::vector<Expression*> args = {x_expr, y_expr};
    auto span = makeSpan(start_loc, previous().location);
    return arena_.make<FunctionCall>(span, function_name, args);
}
```

**Reference Files**:
- Existing patterns: `src/parser/parser.cpp` lines 2300-2500 (function parsing)
- Spatial infrastructure: `include/scratchbird/spatial/wkt_parser.h`

### 2. Bytecode Generation (src/sblr/bytecode_generator.cpp)

Add bytecode generation for spatial functions in `visit(FunctionCall* node)`:

**Opcodes to Generate** (already defined in opcodes.h):
- `EXT_ST_POINT` (0x53) - Create POINT from x,y
- `EXT_ST_MAKELINE` (0x54) - Create LINESTRING from points
- `EXT_ST_MAKEPOLYGON` (0x55) - Create POLYGON from linestring
- `EXT_ST_ASTEXT` (0x56) - Convert geometry to WKT
- `EXT_ST_ASBINARY` (0x57) - Convert geometry to WKB
- `EXT_ST_GEOMETRYTYPE` (0x58) - Get type name
- `EXT_ST_ISVALID` (0x59) - Validate geometry

**Pattern to Follow**:
```cpp
// In visit(FunctionCall* node) around line 1650
if (node->name == "ST_POINT") {
    // Generate code for arguments (x, y)
    node->args[0]->accept(this);  // x coordinate
    node->args[1]->accept(this);  // y coordinate

    // Emit opcode
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_POINT));
}

if (node->name == "ST_ASTEXT") {
    // Generate code for geometry argument
    node->args[0]->accept(this);

    // Emit opcode
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_ASTEXT));
}
```

**Reference Files**:
- Existing patterns: `src/sblr/bytecode_generator.cpp` lines 1600-1700
- Opcode definitions: `include/scratchbird/sblr/opcodes.h` lines 220-230

### 3. Executor Handlers (src/sblr/executor.cpp)

Add opcode handlers in the `execute()` switch statement's `EXTENDED_OPCODE` section:

**Handlers to Implement** (~300-400 lines total):

```cpp
// In execute() switch, after case Opcode::EXTENDED_OPCODE:
case Opcode::EXT_ST_POINT: {
    // Pop y, x from stack (reverse order)
    Value y_val = pop();
    Value x_val = pop();

    if (y_val.isNull() || x_val.isNull()) {
        push(Value::makeNull());
        break;
    }

    double x = x_val.toDouble();
    double y = y_val.toDouble();

    Point point{x, y};
    push(Value::makePoint(point));
    break;
}

case Opcode::EXT_ST_MAKELINE: {
    // This is more complex - need to pop all points from array
    // For now, implement with 2 points (can extend later)
    Value point2 = pop();
    Value point1 = pop();

    if (point1.isNull() || point2.isNull()) {
        push(Value::makeNull());
        break;
    }

    if (point1.type() != DataType::POINT || point2.type() != DataType::POINT) {
        push(Value::makeNull());
        break;
    }

    LineString linestring;
    linestring.points.push_back(point1.getPoint());
    linestring.points.push_back(point2.getPoint());

    if (linestring.isValid()) {
        push(Value::makeLineString(linestring));
    } else {
        push(Value::makeNull());
    }
    break;
}

case Opcode::EXT_ST_ASTEXT: {
    Value geom = pop();

    if (geom.isNull()) {
        push(Value::makeNull());
        break;
    }

    std::string wkt;
    try {
        if (geom.type() == DataType::POINT) {
            wkt = spatial::WKTParser::pointToWKT(geom.getPoint());
        } else if (geom.type() == DataType::LINESTRING) {
            wkt = spatial::WKTParser::lineStringToWKT(geom.getLineString());
        } else if (geom.type() == DataType::POLYGON) {
            wkt = spatial::WKTParser::polygonToWKT(geom.getPolygon());
        } else {
            push(Value::makeNull());
            break;
        }
        push(Value::makeVarChar(wkt));
    } catch (...) {
        push(Value::makeNull());
    }
    break;
}

case Opcode::EXT_ST_ASBINARY: {
    Value geom = pop();

    if (geom.isNull()) {
        push(Value::makeNull());
        break;
    }

    std::vector<uint8_t> wkb;
    try {
        if (geom.type() == DataType::POINT) {
            wkb = spatial::WKBSerializer::serializePoint(geom.getPoint());
        } else if (geom.type() == DataType::LINESTRING) {
            wkb = spatial::WKBSerializer::serializeLineString(geom.getLineString());
        } else if (geom.type() == DataType::POLYGON) {
            wkb = spatial::WKBSerializer::serializePolygon(geom.getPolygon());
        } else {
            push(Value::makeNull());
            break;
        }
        // Convert to BYTEA or BLOB value
        // For now, store as binary string
        std::string binary_str(reinterpret_cast<const char*>(wkb.data()), wkb.size());
        push(Value::makeVarChar(binary_str));  // Or makeBytea if available
    } catch (...) {
        push(Value::makeNull());
    }
    break;
}

case Opcode::EXT_ST_GEOMETRYTYPE: {
    Value geom = pop();

    if (geom.isNull()) {
        push(Value::makeNull());
        break;
    }

    std::string type_name;
    if (geom.type() == DataType::POINT) {
        type_name = "POINT";
    } else if (geom.type() == DataType::LINESTRING) {
        type_name = "LINESTRING";
    } else if (geom.type() == DataType::POLYGON) {
        type_name = "POLYGON";
    } else {
        push(Value::makeNull());
        break;
    }

    push(Value::makeVarChar(type_name));
    break;
}

case Opcode::EXT_ST_ISVALID: {
    Value geom = pop();

    if (geom.isNull()) {
        push(Value::makeNull());
        break;
    }

    bool is_valid = false;
    if (geom.type() == DataType::POINT) {
        // Points are always valid
        is_valid = true;
    } else if (geom.type() == DataType::LINESTRING) {
        is_valid = geom.getLineString().isValid();
    } else if (geom.type() == DataType::POLYGON) {
        is_valid = geom.getPolygon().isValid();
    }

    push(Value::makeBoolean(is_valid));
    break;
}
```

**Reference Files**:
- Existing patterns: `src/sblr/executor.cpp` lines 4500-5000 (extended opcodes)
- WKT infrastructure: `include/scratchbird/spatial/wkt_parser.h`, `src/spatial/wkt_parser.cpp`
- WKB infrastructure: `include/scratchbird/spatial/wkb.h`, `src/spatial/wkb.cpp`
- Value methods: `include/scratchbird/core/types.h` (makePoint, getPoint, etc.)

### 4. Include Headers

Make sure to add necessary includes to executor.cpp:

```cpp
#include "scratchbird/spatial/wkt_parser.h"
#include "scratchbird/spatial/wkb.h"
```

### 5. Testing

**Verify Existing Tests Still Pass**:
```bash
cd build
make wave1_tests
./tests/wave1_tests  # All 44 spatial tests should still pass
```

**Test End-to-End SQL Queries**:

Create a simple test program or use existing test framework to verify:

```sql
-- Test ST_Point
SELECT ST_AsText(ST_Point(1.5, 2.5));
-- Expected: "POINT(1.5 2.5)"

-- Test ST_GeometryType
SELECT ST_GeometryType(ST_Point(0, 0));
-- Expected: "POINT"

-- Test ST_IsValid
SELECT ST_IsValid(ST_Point(1.0, 2.0));
-- Expected: true

-- Test in table context
CREATE TABLE test_locations (id INTEGER, location POINT);
INSERT INTO test_locations VALUES (1, ST_Point(10.5, 20.5));
SELECT id, ST_AsText(location) FROM test_locations;
-- Expected: 1 | "POINT(10.5 20.5)"
```

## Success Criteria

✅ **Parser**:
- ST_Point(x, y) parses correctly
- ST_AsText(geom) parses correctly
- All 7 spatial functions parse

✅ **Bytecode**:
- All 7 spatial opcodes generate correctly
- No compilation errors

✅ **Executor**:
- All 7 opcode handlers work
- ST_Point creates valid Point values
- ST_AsText produces correct WKT
- ST_AsBinary produces correct WKB

✅ **Tests**:
- All 44 existing spatial tests still pass
- End-to-end SQL queries work
- Can CREATE TABLE with POINT columns
- Can INSERT using ST_Point()
- Can SELECT and display using ST_AsText()

## Deliverable

Return a summary including:
1. Files modified and line counts
2. Test results (44/44 passing + any new tests)
3. Example SQL queries that now work
4. Any issues encountered and how you resolved them

## Implementation Notes

- Follow existing code patterns exactly
- Use the WKT/WKB infrastructure that's already complete
- Handle NULL values properly in all functions
- Add error handling with try-catch where needed
- Test incrementally as you implement

Work autonomously and implement this feature completely. Take your time to do it right!
"""
)
```

---

## Option 2: Three Sequential Agents (Alternative)

**Why Consider**:
- Breaks work into smaller, focused chunks
- Each agent has clearer scope
- Easier to debug if one fails

**Why NOT Recommended**:
- Coordination overhead between agents
- Risk of inconsistencies
- Agent 2 depends on Agent 1, Agent 3 depends on both
- Total time similar to Option 1

**If you choose this approach**:

### Agent 1: Parser Only (2 hours)
- Add spatial function parsing
- Test that parsing works

### Agent 2: Bytecode Only (1.5 hours)
- Add bytecode generation
- Test that opcodes emit correctly

### Agent 3: Executor Only (2.5 hours)
- Add opcode handlers
- Test end-to-end

---

## Option 3: Hybrid Approach (Middle Ground)

**Split into 2 agents**:

### Agent A: Core Functions (4 hours)
- ST_Point, ST_AsText, ST_GeometryType
- Parser + bytecode + executor
- Simpler functions, establish pattern

### Agent B: Advanced Functions (2 hours)
- ST_MakeLine, ST_MakePolygon, ST_AsBinary, ST_IsValid
- Follows Agent A's patterns
- More complex logic

---

## Recommendation: Go with Option 1 (Single Agent)

**Reasons**:
1. ✅ **Fastest**: 1-2 hours vs 6-8 hours manual
2. ✅ **Simplest**: No coordination needed
3. ✅ **Most Reliable**: Agent can test and fix as it goes
4. ✅ **Best Results**: Agent 1 delivered 1,703 lines flawlessly
5. ✅ **Infrastructure Ready**: All hard work already done

**What Could Go Wrong**:
- Agent might make small mistakes → You review and fix (15-30 min)
- Agent might miss edge cases → Tests will catch them
- Agent might need guidance → You can provide feedback

**Risk Mitigation**:
- Comprehensive task specification (provided above)
- Clear reference patterns (JSON functions, etc.)
- Existing tests will validate (44 tests must still pass)
- Infrastructure already proven (100% working)

---

## How to Launch

```python
# Launch single comprehensive agent
agent_result = Task(
    subagent_type="general-purpose",
    description="Complete spatial types SQL integration",
    prompt="""[Use the complete prompt from Option 1 above]"""
)
```

**Then**:
1. Agent works autonomously (1-2 hours execution time)
2. Review results when agent completes
3. Test with `make wave1_tests && ./tests/wave1_tests`
4. Fix any minor issues (if needed)
5. Commit and celebrate! 🎉

---

## Expected Outcome

After agent completes:
- ✅ 7 spatial functions work in SQL
- ✅ Can CREATE TABLE with POINT/LINESTRING/POLYGON
- ✅ Can INSERT spatial data using ST_Point(), etc.
- ✅ Can SELECT and display using ST_AsText()
- ✅ All 44 tests still passing
- ✅ ~300-500 lines of integration code added

**Total**: Task 9.1 fully complete, production-ready spatial types!

---

## Next After Task 9

Once Task 9.1 SQL integration is complete:

**Option A**: Continue with Task 9.2 (R-tree Index) - Wave 2
**Option B**: Complete remaining Wave 1 features (Arrays, Text)
**Option C**: Launch all of Wave 2 in parallel

**Recommended**: Complete all Wave 1 features first (Arrays + Text) for maximum momentum, then launch Wave 2 fresh.
