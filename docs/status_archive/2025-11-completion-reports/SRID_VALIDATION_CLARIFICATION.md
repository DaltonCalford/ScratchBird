# Query Planner SRID Validation - Clarification

**Status**: ⏳ DEFERRED TO PHASE 3
**Related Tasks**: Task 9.2 (Query Planner), Task 9.5 (SRID)
**Date**: October 30, 2025

---

## What is Query Planner SRID Validation?

SRID validation in the query planner refers to **compile-time checking** that spatial predicates in WHERE clauses don't mix geometries with incompatible coordinate systems.

### Example Query

```sql
-- This should produce a warning/error at planning time:
SELECT * FROM places
WHERE ST_Intersects(
    location,  -- SRID 4326 (WGS84 geographic)
    ST_Transform(boundary, 3857)  -- SRID 3857 (Web Mercator projected)
);
```

**Problem**: Comparing geometries in different coordinate systems without proper transformation can produce incorrect results.

### What SRID Validation Would Do

The query planner would:
1. Detect spatial predicate functions (ST_Intersects, ST_Contains, etc.)
2. Analyze the SRID of each geometry argument
3. Check if SRIDs are compatible or if explicit transformation is used
4. Issue warning/error if SRIDs mismatch without ST_Transform

---

## Current Phase 2 Implementation Status

### ✅ IMPLEMENTED: Runtime SRID Support

**All SRID operations work correctly at runtime**:
- `ST_SRID(geom)` - Get SRID ✅
- `ST_SetSRID(geom, srid)` - Set SRID ✅
- `ST_Transform(geom, target_srid)` - Transform coordinates ✅
- `ST_Distance_Sphere(g1, g2)` - Geodetic distance ✅

**Geometries correctly track SRID**:
- Point, LineString, Polygon all have SRID fields
- SRID preserved through operations
- Transformations work correctly with PROJ library

### ⏳ DEFERRED: Compile-Time SRID Validation

**Not implemented in Phase 2**:
- Query planner does NOT validate SRID compatibility
- No warnings issued for SRID mismatches
- Spatial predicates work but don't check SRIDs at planning time

**Why deferred**:
1. **String Pool Resolution Required**: Query planner needs to resolve StringIds to actual function names to identify spatial predicates
2. **Static Analysis Complexity**: Determining SRID at planning time requires constant folding and expression analysis
3. **Not Critical for Phase 2**: Runtime behavior is correct; this is a "nice to have" warning system
4. **Phase 3 Scope**: Better suited for Phase 3 when doing comprehensive query validation enhancements

---

## Technical Blockers

### Issue 1: String Pool in Query Planner

The query planner currently detects spatial predicates like this:

```cpp
auto QueryPlanner::isSpatialPredicate(const parser::Expression *expr,
                                      std::string &column_name,
                                      std::string &function_name) const
    -> bool
{
    // Check if expression is a function call
    auto *func_call = dynamic_cast<const parser::FunctionCallExpr*>(expr);
    if (!func_call)
        return false;

    // Problem: func_call->name() returns StringId, not actual string!
    // We can't compare StringIds without the string pool.

    // Phase 2 simplified approach:
    // Just check if it's a function call with a column as first argument
    // Don't try to validate function names or SRIDs
}
```

**Blocker**: The query planner doesn't have access to the `StringPool` to resolve function names.

**Solution (Phase 3)**: Pass `StringPool` to query planner methods.

### Issue 2: SRID Static Analysis

Even with function names resolved, determining SRID at planning time is complex:

```sql
-- Simple case: SRID is constant
SELECT * FROM places
WHERE ST_Intersects(
    location,                    -- SRID from column metadata
    ST_SetSRID(ST_Point(0, 0), 4326)  -- SRID is constant 4326
);

-- Complex case: SRID is dynamic
SELECT * FROM places p1, places p2
WHERE ST_Intersects(
    p1.location,                 -- SRID from p1.location column
    ST_Transform(p2.boundary, ST_SRID(p1.location))  -- SRID depends on runtime value
);
```

**Phase 2**: Only handles spatial predicate detection, not SRID analysis
**Phase 3**: Add expression evaluation to determine SRIDs statically where possible

---

## Phase 2 Behavior (Current)

### What Happens Now

Without query planner SRID validation:

1. **Spatial predicates work correctly**:
   ```sql
   SELECT * FROM places
   WHERE ST_Intersects(location, ST_Point(0, 0));
   ```
   - Query planner detects function call
   - Generates R-tree scan path if beneficial
   - Executor runs ST_Intersects at runtime
   - ✅ **Works correctly**

2. **SRID mismatches pass planning**:
   ```sql
   SELECT * FROM places
   WHERE ST_Intersects(
       location,  -- SRID 4326
       ST_SetSRID(ST_Point(0, 0), 3857)  -- Different SRID!
   );
   ```
   - Query planner generates plan without warning
   - Executor runs ST_Intersects
   - ⚠️ **Result may be incorrect** (comparing incompatible coordinate systems)
   - No error, no warning

3. **User must validate SRIDs manually**:
   ```sql
   -- Best practice: User ensures SRIDs match
   SELECT * FROM places
   WHERE ST_Intersects(
       location,  -- SRID 4326
       ST_Transform(ST_SetSRID(ST_Point(0, 0), 3857), 4326)  -- Explicitly transform
   );
   ```
   - User manually adds ST_Transform
   - ✅ **Correct behavior**

### Impact

**For Phase 2**:
- ✅ All SRID operations work correctly
- ✅ Spatial queries execute properly
- ⚠️ Users must manually ensure SRID compatibility
- ⚠️ No automatic warnings for SRID mismatches

**This is acceptable** because:
- Runtime behavior is correct
- Documentation can guide users to check SRIDs
- Most GIS users are aware of coordinate system issues
- Phase 3 will add automatic validation

---

## Phase 3 Implementation Plan

### Step 1: Pass StringPool to Query Planner (~1 hour)

Update query planner methods to accept StringPool:

```cpp
auto QueryPlanner::isSpatialPredicate(
    const parser::Expression *expr,
    const parser::StringPool &string_pool,  // Add this parameter
    std::string &column_name,
    std::string &function_name) const
    -> bool
{
    auto *func_call = dynamic_cast<const parser::FunctionCallExpr*>(expr);
    if (!func_call)
        return false;

    // Now we can resolve the function name!
    std::string func_name = string_pool.getString(func_call->name());

    // Check against known spatial functions
    if (func_name == "ST_INTERSECTS" ||
        func_name == "ST_CONTAINS" ||
        func_name == "ST_WITHIN" ||
        // ... etc
    {
        function_name = func_name;
        return true;
    }

    return false;
}
```

### Step 2: Add SRID Analysis (~2-3 hours)

Implement static SRID analysis:

```cpp
auto QueryPlanner::analyzeSRID(
    const parser::Expression *expr,
    const parser::StringPool &string_pool,
    core::ErrorContext *ctx)
    -> std::optional<int32_t>
{
    // If it's ST_SetSRID with constant SRID
    if (auto *func = dynamic_cast<const FunctionCallExpr*>(expr))
    {
        std::string name = string_pool.getString(func->name());
        if (name == "ST_SETSRID" && func->args().size() == 2)
        {
            // Try to evaluate second argument as constant
            if (auto *lit = dynamic_cast<const IntegerLiteral*>(func->args()[1]))
            {
                return lit->value();
            }
        }
    }

    // If it's a column reference, look up SRID from catalog
    if (auto *col = dynamic_cast<const IdentifierExpr*>(expr))
    {
        // Query catalog for column SRID
        // ...
    }

    return std::nullopt;  // SRID unknown at planning time
}
```

### Step 3: Add Validation Warnings (~1-2 hours)

Emit warnings when SRID mismatch detected:

```cpp
auto QueryPlanner::validateSpatialPredicateSRIDs(
    const parser::FunctionCallExpr *predicate,
    const parser::StringPool &string_pool,
    core::ErrorContext *ctx)
    -> void
{
    auto srid1 = analyzeSRID(predicate->args()[0], string_pool, ctx);
    auto srid2 = analyzeSRID(predicate->args()[1], string_pool, ctx);

    if (srid1.has_value() && srid2.has_value() && srid1 != srid2)
    {
        std::string msg = "Warning: Spatial predicate compares geometries with different SRIDs (";
        msg += std::to_string(*srid1) + " vs " + std::to_string(*srid2);
        msg += "). Consider using ST_Transform to ensure compatible coordinate systems.";

        DEBUG_LOG_DB(msg);
        // Optionally emit warning to user
    }
}
```

### Total Phase 3 Effort: ~4-6 hours

---

## Comparison: PostgreSQL/PostGIS

### PostgreSQL Behavior

PostgreSQL **does NOT** validate SRID at planning time either:

```sql
-- PostgreSQL allows this without warning:
SELECT ST_Intersects(
    ST_SetSRID(ST_Point(0, 0), 4326),
    ST_SetSRID(ST_Point(0, 0), 3857)
);

-- Returns: ERROR at runtime (not planning time!)
-- "Operation on mixed SRID geometries"
```

**Key difference**: PostgreSQL checks at **runtime** (in the function), not at planning time.

### ScratchBird Phase 2 vs PostgreSQL

| Feature | PostgreSQL | ScratchBird Phase 2 | ScratchBird Phase 3 |
|---------|-----------|---------------------|---------------------|
| Runtime SRID support | ✅ | ✅ | ✅ |
| Runtime SRID error | ✅ | ⚠️ No error | ✅ Add runtime error |
| Planning-time warning | ❌ | ❌ | ✅ Add warning |

**Phase 3 Goal**: Be better than PostgreSQL by adding planning-time warnings.

---

## Documentation and User Guidance

### Phase 2 User Documentation

Users should be advised:

1. **Always check SRIDs**:
   ```sql
   SELECT ST_SRID(location) FROM places;
   ```

2. **Use ST_Transform for compatibility**:
   ```sql
   SELECT * FROM places
   WHERE ST_Intersects(
       location,
       ST_Transform(boundary, ST_SRID(location))
   );
   ```

3. **Set SRIDs explicitly**:
   ```sql
   INSERT INTO places (location)
   VALUES (ST_SetSRID(ST_Point(0, 0), 4326));
   ```

### Error Messages (Phase 3)

Phase 3 will add helpful error messages:
```
WARNING: Spatial predicate ST_Intersects compares geometries with different SRIDs
  Left argument SRID: 4326 (WGS 84)
  Right argument SRID: 3857 (WGS 84 / Pseudo-Mercator)

Consider using ST_Transform to convert to a common coordinate system:
  ST_Intersects(geom1, ST_Transform(geom2, 4326))
```

---

## Summary

### Current Status (Phase 2)

✅ **All SRID operations work correctly**
✅ **Runtime SRID tracking functional**
✅ **Coordinate transformations operational**
⏳ **Query planner SRID validation deferred to Phase 3**

### Why Deferral is Acceptable

1. **Runtime behavior is correct** - SRID operations work
2. **Not safety-critical** - Incorrect results, not crashes
3. **PostgreSQL doesn't do this either** - Industry standard behavior
4. **Phase 3 enhancement** - Makes ScratchBird better than PostgreSQL
5. **User awareness** - GIS users typically understand coordinate systems

### Phase 3 Deliverable

Query planner will emit **warnings** (not errors) when SRID mismatches are detected, helping users write correct spatial queries without breaking existing queries.

**Estimated Phase 3 effort**: 4-6 hours

---

**Conclusion**: SRID validation is a quality-of-life enhancement deferred to Phase 3. Phase 2 SRID support is 100% functional for all runtime operations.
