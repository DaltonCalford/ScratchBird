# V3 Spec Clarity Review

Date: 2026-02-08

This report lists ambiguous, underspecified, or implementation-guess areas found while reviewing V3 specs.
Each entry includes the spec path, the unclear point, and the missing detail needed for an unambiguous implementation.

## Findings

### /docs/specifications/parser/v3/ARCHITECTURE_CLARIFICATIONS.md
- Potential ambiguity markers detected:
  - L455: startup_database.sales = null  (or not specified)
  - L533: // 5. Cluster Manager Thread (optional)
  - L567: │    ├──▶ Cluster Thread (optional) │
  - L620: │  │  - May leave locks dangling     │   │
  - L629: - Bad connection may ignore request
  - L631: - Locks may not be released properly
  - L915: - Server: "Last query status: UNKNOWN (server error)"
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/AST_TYPE_AND_LITERAL_SPEC.md
- Potential ambiguity markers detected:
  - L13: 1. **AST nodes must carry resolved catalog IDs** for all catalog-backed types (domain, enum, set, row/composite). Names may be kept for diagnostics but **SBLR emission uses IDs only**.
  - L26: - precision: u32 (optional; 0 if not applicable)
  - L27: - scale: u32 (optional; 0 if not applicable)
  - L41: - `UNKNOWN`
  - L84: - fields: [FieldSpec] (optional inline definition if catalog is not yet bound)
  - L101: - srid: u32 (0 = unspecified)
  - L114: - bit_length: u16 (0 = unspecified)
  - L179: - label: string (optional if ordinal present)
  - L380: - tz_name: string (optional; canonical per `types/CANONICALIZATION_RULES.md`)
  - L391: - tz_name: string (optional; canonical per `types/CANONICALIZATION_RULES.md`)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md
- Potential ambiguity markers detected:
  - L98: - No authoritative file may be omitted.
  - L99: - No non‑authoritative file may appear here.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/IMPLEMENTATION_AUDIT.md
- Potential ambiguity markers detected:
  - L102: - TODO: Add `original_user_id_` and `effective_user_id_` fields to ConnectionContext
  - L176: uint32_t using_expr_oid;           // TOAST reference for USING expression (TODO: Phase 3.4.6)
  - L177: uint32_t with_check_expr_oid;      // TOAST reference for WITH CHECK expression (TODO: Phase 3.4.6)
  - L225: // TODO: Implement role membership filtering (currently returns all policies)
  - L330: //   Features: Optional IF EXISTS, optional CASCADE/RESTRICT
  - L389: //   TODO: Expression evaluation (currently stores empty strings)
  - L410: // Returns: true if RLS should be enforced, false if bypassed
  - L683: - `Status alterSequence(const ID& sequence_id, optional<int64_t> increment_by, ..., ErrorContext* ctx)` → h:785
  - L1119: // Determine if RLS should be enforced for current user/table
  - L1161: 1. Check if RLS should be enforced (bypass if not)
  - L1183: **Note**: PolicyInfo.roles currently stores role NAMES (should migrate to UUIDs for O(1) lookup)
  - L1184: **TODO**: Transitive role membership (groups)
  - L1206: // 1. Construct full row_values with defaults for unspecified columns
  - L1457: // TODO: Use B-Tree index for O(log n) lookup when parser supports UNIQUE
  - L1495: parent_columns = parseColumnList();  // Optional
  - L1506: // Optional: CONSTRAINT name
  - L1513: - `TableConstraint` base class (line ~TBD)
  - L1797: std::vector<std::string> column_names;  // Optional explicit columns
  - L1928: - TODO: Parse and execute SELECT query, populate physical table
  - L1929: - TODO: Implement CONCURRENTLY option (temp table + atomic swap)
  - ... 2 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/INTEGRATION_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L146: - `ST_Dump`: Requires set-returning function support (may need SBLR enhancement)
  - L243: **Note**: May need to create `/home/user/ScratchBird/src/spatial/wkt_parser.cpp` if WKT parsing doesn't exist.
  - L291: - May need to use `boost::multiprecision` or custom 128-bit string conversion
  - L496: **Note**: May need to create new Expression subclasses:
  - L518: - (Optional) Literal syntax parsing
  - L718: - **INT128 String Conversion**: Platform-specific, may need custom implementation
  - L719: - **Set-Returning Functions**: ST_Dump may require SBLR enhancements
  - L722: - **Parser Literal Syntax**: Multi-geometry literals are complex, may defer to function syntax
  - L746: - ✅ (Optional) Literal syntax parses correctly
  - L766: 3. **ST_Dump Set-Returning**: May need SBLR enhancements
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/BITMAP_INDEX_COMPLETION_PLAN.md
- Potential ambiguity markers detected:
  - L483: - [x] Current implementation uses TransactionId (uint64_t)
  - L498: - **Problem**: Large bitmaps may not fit in memory
  - L503: - **Problem**: Bitwise operations may be slow for large bitmaps
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/BRIN_INDEX_COMPLETION_PLAN.md
- Potential ambiguity markers detected:
  - L173: // 3. TODO Phase 2: Traverse all BRIN pages via sibling pointers
  - L198: 2. Vacuum with no dead ranges (should be no-op)
  - L620: - [x] Current implementation uses TransactionId (uint64_t)
  - L641: - **Problem**: Linear scan across pages may be slow
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/COLUMNSTORE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L473: - Use SIMD instructions if available (optional)
  - L862: - **Detection**: Grep for `Snapshot` in code (should be ZERO occurrences)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/GIST_INDEX_COMPLETION_PLAN.md
- Potential ambiguity markers detected:
  - L335: 1. Penalty for contained entry (should be zero)
  - L336: 2. Penalty for distant entry (should be large)
  - L528: #### Task 4.3: Implement Full Index Rebuild (Optional) (2-3 hours)
  - L553: **Estimated Effort**: 2-3 hours (OPTIONAL)
  - L604: - [ ] Task 4.3: Implement full index rebuild (2-3h) [OPTIONAL]
  - L623: - [x] Current implementation uses TransactionId (uint64_t)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/HNSW_INDEX_COMPLETION_PLAN.md
- Potential ambiguity markers detected:
  - L413: #### Task 3.2: Optional: Sample-Based Path Length (2-4 hours)
  - L439: **Estimated Effort**: 2-4 hours (OPTIONAL)
  - L485: - [ ] Task 3.2: Optional: Sample-based path length (2-4h) [OPTIONAL]
  - L499: - [x] Current implementation uses TransactionId (uint64_t)
  - L525: - **Problem**: Link operations may slow inserts
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/LSM_TREE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L1082: - **Detection**: Grep for `Snapshot` in code (should be ZERO occurrences)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/LSM_TREE_INTEGRATION_PLAN.md
- Potential ambiguity markers detected:
  - L164: std::optional<IndexType> parseIndexType(const std::string &type_str)
  - L187: return (it != type_map.end()) ? std::optional<IndexType>(it->second) : std::nullopt;
  - L207: default: return "UNKNOWN";
  - L256: std::optional<std::string> index_type_;  // "BTREE", "LSM", etc.
  - L258: std::optional<std::string> indexType() const { return index_type_; }
  - L266: - [x] Default is BTREE if not specified
  - L719: return 100.0;  // Unknown predicate type
  - L843: // LSM-Tree should be faster for write-heavy workloads
  - L978: ### Medium Priority (Should Have)
  - L984: 8. **Performance comparison tests** - Optional benchmarking
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/Index_Implementation_Archive/SPGIST_INDEX_COMPLETION_PLAN.md
- Potential ambiguity markers detected:
  - L514: - [x] Current implementation uses TransactionId (uint64_t)
  - L539: - **Problem**: Dynamic partitioning overhead may slow inserts
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/00_COMPREHENSIVE_AUDIT_PLAN.md
- Potential ambiguity markers detected:
  - L18: 4. **Deferred Work**: Find all TODO/FIXME/DEFERRED markers
  - L47: - [ ] Search all source for TODO
  - L81: 4. `04_DEFERRED_WORK_INVENTORY.md` - All TODO/FIXME/DEFERRED
  - L104: - [x] Phase 4: Deferred work inventory (TODO/FIXME/DEFERRED) - **COMPLETE**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/04_DEFERRED_WORK_CORRECTED_ASSESSMENT.md
- Potential ambiguity markers detected:
  - L181: // TODO: Implement HNSW graph insertion
  - L235: // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
  - L329: **ALPHA Impact**: ⚠️ **REVIEW NEEDED** - May affect transaction correctness
  - L335: **ALPHA Impact**: ⚠️ **REVIEW NEEDED** - May affect concurrency correctness
  - L364: - ⚠️ Connection Context Transaction Cleanup (5-10 hours) - May affect correctness
  - L365: - ⚠️ Lock Manager Per-Proc Tracking (20-30 hours) - May affect concurrency
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/04_DEFERRED_WORK_INVENTORY.md
- Potential ambiguity markers detected:
  - L8: **Scope**: All TODO/FIXME/DEFERRED markers in source code
  - L15: The codebase contains **105 TODO/FIXME/DEFERRED markers** indicating incomplete or deferred functionality. These have been categorized by severity and component.
  - L63: // TODO: Implement HNSW graph insertion
  - L95: // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
  - L240: | Component | TODO Count | Critical | High | Medium | Low |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/07_ALPHA_COMPLETION_ROADMAP.md
- Potential ambiguity markers detected:
  - L83: **Status**: **105 TODO/FIXME markers found**
  - L294: - Some memory leaks may persist
  - L313: - Memory leaks may persist
  - L453: 1. **MGA compliance fix scope**: May uncover more issues (Mitigation: Early testing)
  - L454: 2. **TOAST index integration complexity**: May affect performance (Mitigation: Benchmarking)
  - L455: 3. **UTF-8 identifier migration**: Existing databases may need conversion (Mitigation: Migration tool)
  - L458: 1. **TOAST GC implementation**: May miss edge cases (Mitigation: Stress testing)
  - L459: 2. **Testing timeline**: May need more time (Mitigation: Parallel testing)
  - L460: 3. **Performance regression**: Fixes may slow queries (Mitigation: Profiling)
  - L464: 2. **Tool integration**: External tools may need updates (Mitigation: Version 2)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_CORE_STORAGE_AUDIT.md
- Potential ambiguity markers detected:
  - L310: The only significant gap is **Tablespaces**, which has data structures defined but zero implementation. This is documented in the code with TODO comments but may mislead users who expect full tablespace support.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_DATA_TYPE_SYSTEM_REPORT.md
- Potential ambiguity markers detected:
  - L76: | CHAR | ✅ YES | string | Variable | Flags + optional precision + length + UTF-8 |
  - L77: | VARCHAR | ✅ YES | string | Variable | Flags + optional precision + length + UTF-8 |
  - L87: | TIMESTAMP | ✅ YES | int64 + metadata | 9-11 (variable) | Flags + optional TZ hint + microseconds |
  - L186: [2-byte timezone_hint] (optional if with_timezone flag set)
  - L196: [4-byte precision] (optional if has_precision flag set)
  - L336: | Any integer | FLOAT32, FLOAT64 | Int to float (may lose precision) ✓ |
  - L350: | Any numeric | Any numeric | May overflow, may truncate |
  - L351: | String | Any type | Requires parsing, may fail |
  - L387: auto int64ToInt8(int64_t v, ErrorContext *ctx) -> std::optional<int8_t>
  - L406: - **No overflow checking** (behavior undefined for out-of-range floats)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_ELEMENT_EXTRACTION_DETAILED.md
- Potential ambiguity markers detected:
  - L32: ErrorContext *ctx = nullptr) -> std::optional<int64_t>;
  - L127: auto operator[](const std::string& key) const -> std::optional<JSONBValue>;
  - L128: auto operator[](size_t index) const -> std::optional<JSONBValue>;
  - L129: auto getPath(const std::string& path) const -> std::optional<JSONBValue>;
  - L134: -> std::optional<JSONBValue>;
  - L181: auto getElement(size_t flat_index) const -> std::optional<Element>;
  - L182: auto at(const std::vector<size_t>& indices) const -> std::optional<Element>;
  - L184: -> std::optional<ArrayValue>;
  - L278: std::optional<T> lower() const;
  - L279: std::optional<T> upper() const;
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_IMPLEMENTATION_GAPS.md
- Potential ambiguity markers detected:
  - L184: view.materialized_table_id = ID{};  // TODO: Create physical table for materialized data
  - L189: // TODO: ALPHA Phase 1 - Implement actual refresh logic:
  - L325: INSERT INTO users VALUES (1, 'user2@example.com');  -- Should fail, doesn't
  - L326: INSERT INTO users VALUES (NULL, 'user3@example.com');  -- Should fail, doesn't
  - L335: **Current Implementation:**
  - L339: **Should Use:**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-18_SECURITY_CRITICAL_ISSUES.md
- Potential ambiguity markers detected:
  - L29: | Operation | Line | TODO Comment | Vulnerability |
  - L56: DROP USER admin;  -- ALLOWED (should be denied)
  - L59: CREATE USER evil_admin WITH SUPERUSER PASSWORD 'password';  -- ALLOWED (should be denied)
  - L62: GRANT ALL PRIVILEGES ON ALL TABLES TO hacker;  -- ALLOWED (should be denied)
  - L65: REVOKE ALL PRIVILEGES ON ALL TABLES FROM PUBLIC;  -- ALLOWED (should be denied)
  - L68: DROP ROLE security_admin;  -- ALLOWED (should be denied)
  - L69: DROP GROUP auditors;  -- ALLOWED (should be denied)
  - L91: // TODO: checkPermission uses a placeholder that "allows all"
  - L96: The `checkPermission()` method may be a placeholder that returns `true` (allow) for all operations. This would affect:
  - L124: // TODO: Implement session user tracking
  - L197: Add permission checks to all 13 TODO locations:
  - L236: - Examine current implementation
  - L243: - Non-superuser attempts to DROP USER (should fail)
  - L244: - Non-owner attempts to GRANT permission (should fail)
  - L245: - Non-owner attempts to create RLS policy (should fail)
  - L246: - Superuser performs all operations (should succeed)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_AUDIT_CORRECTIONS_REPORT.md
- Potential ambiguity markers detected:
  - L99: **Audit Claim**: "❌ remove(): STUB - Marked 'TODO Phase 5'"
  - L253: ### Medium-Term Actions (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_CONSTRAINT_SYSTEM_CRITICAL_ISSUES.md
- Potential ambiguity markers detected:
  - L35: // SHOULD EXIST but DOESN'T:
  - L41: **Test Case That Should Fail But Passes**:
  - L44: INSERT INTO users (id, name) VALUES (NULL, NULL);  -- Should ERROR but doesn't!
  - L67: // SHOULD EXIST but DOESN'T:
  - L77: **Test Case That Should Fail But Passes**:
  - L80: INSERT INTO products (id, price) VALUES ('hello', 'world');  -- Should ERROR but doesn't!
  - L107: // SHOULD EXIST but DOESN'T:
  - L162: **Current Implementation** (lines 16492-16521):
  - L176: **Should Be** (using index):
  - L211: // TODO: Load expression from TOAST
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_DATA_TYPE_SYSTEM_AUDIT.md
- Potential ambiguity markers detected:
  - L701: -> std::optional<int128_t>;
  - L704: -> std::optional<uint8_t>;
  - L708: -> std::optional<int64_t>;
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_EXECUTIVE_SUMMARY.md
- Potential ambiguity markers detected:
  - L70: - 9 TODO comments for permission checks in DDL operations
  - L160: - **Stub (2)**: HNSW (remove() TODO), BRIN (search/remove stubs)
  - L401: - Add negative tests (constraint violations should fail)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-19_INDEX_SYSTEM_DETAILED_REPORT.md
- Potential ambiguity markers detected:
  - L43: // Line 886: TODO: Use xid to set btn_xmax instead of physical removal
  - L50: - Should set `btn_xmax = xid` (MGA approach)
  - L231: - ❌ **remove()**: STUB - Marked "TODO Phase 5"
  - L236: **Critical Issue**: remove() is a stub marked "TODO Phase 5"
  - L239: // TODO Phase 5: Implement HNSW deletion
  - L397: | HNSW | ✅ | ✅ | ❌ TODO | N/A | ✅ | ❌ | 60% | STUB |
  - L432: **Critical Issue**: Line 886 TODO indicates physical deletion
  - L440: **Should Be**:
  - L459: - Marked "TODO Phase 5"
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_COMPREHENSIVE_AUDIT_EXECUTIVE_SUMMARY.md
- Potential ambiguity markers detected:
  - L181: - ⚠️ 97 NOT_IMPLEMENTED/TODO markers
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_CONSTRAINT_ENFORCEMENT_VERIFICATION.md
- Potential ambiguity markers detected:
  - L227: // ALPHA Phase A: Fill in DEFAULT values or NULL for columns not specified in INSERT
  - L566: INSERT INTO t VALUES (NULL);  -- Should FAIL ✓
  - L567: INSERT INTO t VALUES (1);     -- Should SUCCEED ✓
  - L568: UPDATE t SET id = NULL;       -- Should FAIL ✓
  - L574: INSERT INTO t VALUES ('string', 123);  -- Should FAIL (type mismatch) ✓
  - L575: INSERT INTO t VALUES (1, 99.99);       -- Should SUCCEED ✓
  - L581: INSERT INTO t VALUES (1);    -- Should SUCCEED ✓
  - L582: INSERT INTO t VALUES (1);    -- Should FAIL (duplicate) ✓
  - L583: INSERT INTO t VALUES (NULL); -- Should FAIL (NULL in PK) ✓
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_EXECUTIVE_SUMMARY.md
- Potential ambiguity markers detected:
  - L138: - **Impact**: **Data integrity violation** - queries may return incorrect results
  - L158: - 🟡 MEDIUM: 6 issues (should fix soon)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_INDEX_SYSTEM_AUDIT.md
- Potential ambiguity markers detected:
  - L82: // TODO: Implement R-Tree removal with logical deletion
  - L228: float distance = computeDistance(query, level_data);  // TODO: Implement distance metrics
  - L231: float dist = computeDistance(entry_key, query);  // TODO: Multiple distance functions
  - L234: float new_dist = computeDistance(query, neighbor_data);  // TODO: Configurable distance
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_INDEX_SYSTEM_AUDIT_CORRECTED.md
- Potential ambiguity markers detected:
  - L103: float distance = computeDistance(query, level_data);  // TODO: Implement distance metrics
  - L106: float dist = computeDistance(entry_key, query);  // TODO: Multiple distance functions
  - L109: float new_dist = computeDistance(query, neighbor_data);  // TODO: Configurable distance
  - L129: - No TODO comments in actual code (lines 831, 907 call the implemented function)
  - L190: - TODO: Key extractor registry (line 19778-19780) - uses nullptr for now
  - L222: **Current Implementation (src/core/rtree_index.cpp):**
  - L265: **Current Implementation (src/core/columnstore_index.cpp):**
  - L325: - \*\*HNSW confirmed PRODUCTION READY: Full implementation with configurable distance metrics. The only TODO is an optional for diversity-based neighbor selection (line 1460), which doesn't block production use.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_MEMORY_SAFETY_AUDIT.md
- Potential ambiguity markers detected:
  - L20: | 🟡 MEDIUM | 6 | Should fix in current release |
  - L486: current_offset = 8 + 3 = 11  (WRONG! Should be 8 + 4GB)
  - L739: 3. **Undefined Behavior Sanitizer (UBSan)** - Detect integer overflows
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_MEMORY_SAFETY_FIXES_IMPLEMENTED.md
- Potential ambiguity markers detected:
  - L209: // Manual release (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_SECURITY_FIXES_IMPLEMENTATION_NOTES.md
- Potential ambiguity markers detected:
  - L149: # Should output: #define HAVE_CRYPT_R 1
  - L237: std::mt19937 gen(rd());  // ❌ May use weak seed if rd has zero entropy
  - L283: // 3. Materialized data should respect the refreshing user's permissions
  - L284: // Current implementation delegates to catalog_manager->refreshMaterializedView()
  - L285: // which should enforce RLS through the query planner. Verify this is working correctly.
  - L288: #### Current Implementation
  - L290: The current implementation delegates to `catalog_manager->refreshMaterializedView()`, which should enforce RLS through the query planner. This needs verification with integration tests.
  - L303: -- As admin (should see all data)
  - L305: SELECT * FROM sensitive_summary;  -- Should contain all users
  - L307: -- As regular user (should only see own data OR fail)
  - L308: REFRESH MATERIALIZED VIEW sensitive_summary;  -- Should fail or filter by RLS
  - L773: # Should succeed with bcrypt support
  - L778: # Both should return: "Invalid username or password"
  - L782: # Should return: "Invalid regular expression" (no library details)
  - L786: # Should be absent on systems with good entropy
  - L790: # Should return: "Invalid XML" (no libxml2 details)
  - L805: | Materialized view RLS | ⚠️ Unknown | MEDIUM |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/2025-11-20_SECURITY_VULNERABILITIES_AUDIT.md
- Potential ambiguity markers detected:
  - L442: Error: XML parsing failed: libxml2 error: namespace prefix 'foo' is not defined (libxml2 2.9.10)
  - L540: return it->second.has_permission;  // Cached result (may be stale!)
  - L668: Row-Level Security (RLS) policies may not be enforced during materialized view refresh, potentially allowing materialized views to contain data that should be filtered by RLS.
  - L734: -- 1       | 100    ← Should NOT be visible to user 2!
  - L982: #### Recommended Fix (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/CATALOG_CRUD_AUDIT.md
- Potential ambiguity markers detected:
  - L323: 2. **Parameters** (0% CRUD) - Should be implemented with Procedures
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/CATALOG_SYSTEM_AUDIT.md
- Potential ambiguity markers detected:
  - L247: | **dropUser** | ❌ MISSING | Not found (TODO: cascading cleanup at line 9055-9134) |
  - L250: **Note**: Password hash storage should use TOAST (TODO comments at 8920, 8962, 8992, 9023)
  - L265: | **dropRole** | ❌ MISSING | Not found (TODO: cascading cleanup at line 9293) |
  - L282: | **dropGroup** | ❌ MISSING | Not found (TODO: cascading cleanup at line 9626-9645) |
  - L685: - **TODO/STUB markers**: 100+ occurrences
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/COLUMNSTORE_100_PERCENT_COMPLETE.md
- Potential ambiguity markers detected:
  - L25: ## Phase 3.1: Multi-Page Segments (FINAL TODO)
  - L126: - Should use single page (existing behavior)
  - L131: - Should split across multiple pages
  - L138: - Should split across ~13,000 pages
  - L140: - Decompression should work correctly
  - L248: ## Phase 4: Catalog Metadata Persistence (FINAL TODO - 6/6)
  - L407: ### Optional Enhancements (Post-100%)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/COLUMNSTORE_PHASE2_3_SUMMARY.md
- Potential ambiguity markers detected:
  - L111: - High-cardinality strings (should fall back to RLE)
  - L177: 3. Scan all → should return 1100 rows
  - L187: 3. Scan column B → should skip A and C segments
  - L230: Existing tests should pass without modification.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/COMPREHENSIVE_CODE_AUDIT_2025-11-20.md
- Potential ambiguity markers detected:
  - L14: **Purpose:** This audit was conducted to verify actual implementation status versus documentation claims, focusing exclusively on executable code and ignoring all comments, documentation, and TODO markers.
  - L628: - "99% complete" should be "80% complete"
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/DATABASE_PAGE_SIZE_AUDIT_REPORT.md
- Potential ambiguity markers detected:
  - L314: ## 3. High Priority Issues (❌ SHOULD FIX)
  - L430: **Issue:** These should use `db_->page_size()` instead
  - L759: # For each occurrence, evaluate if it should be:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/DATABASE_PAGE_SIZE_AUDIT_REPORT_UPDATED.md
- Potential ambiguity markers detected:
  - L552: **Next Steps:** Optional - Add comprehensive test suite for all page sizes
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/DOCUMENTATION_DISCREPANCY_REPORT.md
- Potential ambiguity markers detected:
  - L150: 2. Distance computation has TODO comments for cosine/Manhattan/dot product
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/EXECUTIVE_SUMMARY.md
- Potential ambiguity markers detected:
  - L114: // INT64_MAX + 1 = undefined behavior
  - L153: // TODO: findAllParallel should accept current_xid
  - L154: // Currently may return invisible tuples (WRONG!)
  - L292: - 100+ TODO markers not tracked
  - L372: **Should Fix:**
  - L431: 3. **Limited testing** - Unknown edge case behavior
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/FIREBIRD_SCRATCHBIRD_FEATURE_COMPARISON.md
- Potential ambiguity markers detected:
  - L75: | **Delta Compression** | Yes (optional) | Yes (optional) | ✅ Identical |
  - L106: | **Compression** | Optional | LZ4 compression | ✅ Compatible |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/FUNCTION_VERIFICATION_REPORT.md
- Potential ambiguity markers detected:
  - L197: - ⚠️ `UNNEST(array)` - Opcode defined but may be a table function
  - L294: **Recommendation**: Math functions should be prioritized as one of the most critical missing features (~40-50 functions needed).
  - L414: However, the **complete absence of mathematical functions** is a critical gap that significantly limits the engine's usefulness for scientific, financial, and analytical applications. This should be the #1 priority for the next development phase.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/IMPROVEMENT_OPPORTUNITIES.md
- Potential ambiguity markers detected:
  - L186: // INT64_MAX + 1 = undefined behavior
  - L210: **Impact:** Prevents data corruption, undefined behavior
  - L241: **Impact:** Prevents undefined behavior, improves IEEE 754 compliance
  - L253: // TODO: findAllParallel should accept current_xid
  - L254: // Parallel queries may return invisible tuples!
  - L287: std::optional<SequenceInfo> getSequence(const std::string& name) {
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_COMPLIANCE_ANALYSIS_SUMMARY.md
- Potential ambiguity markers detected:
  - L44: $ grep -c "TODO" src/core/rtree_index.cpp
  - L103: **Note**: Columnstore may legitimately need specialized bulk ops - needs review
  - L111: 1. **GIN - Line 616**: `// TODO: Implement in Phase 4`
  - L116: 2. **HNSW - Line 1460**: `// TODO: Implement more sophisticated heuristic`
  - L121: 3. **LSM-Tree - Line 710**: `// TODO: Add proper logging`
  - L147: **Impact**: Indexes may have untested edge cases, especially MGA compliance
  - L163: // TODO: Implement tree traversal
  - L191: 5. Remove 10 TODO stubs
  - L274: // May not be semantically correct for analytics workload
  - L354: **Impact**: MEDIUM (R-Tree ops may fail at runtime)
  - L359: **Impact**: MEDIUM (vector search may produce incorrect results)
  - L377: - ✅ R-Tree TODO count: 10 → 0
  - L379: - ✅ Minor TODO count: 3 → 0
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_COMPLIANCE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L43: 8. ✅ Remove all 10 TODO stubs
  - L52: - All TODO markers removed
  - L154: **Decision**: If Columnstore is designed for bulk analytics loads, generic row-level bytecode may not apply. Document this clearly.
  - L170: - **Action**: Review code, implement or remove TODO
  - L219: // Search with current_xid=101 (should see)
  - L220: // Search with current_xid=99 (should NOT see)
  - L226: // Search with current_xid=103 (should NOT see)
  - L255: - ⚠️ One minor TODO (line 710: logging)
  - L261: 1. ✅ Complete logging TODO (see Issue 2.1)
  - L339: **Impact**: HIGH - May not be able to implement generic bytecode
  - L345: **Risk**: Creating 5 new test files may take longer than estimated
  - L352: **Risk**: BoundingBox conversion may have edge cases
  - L353: **Impact**: MEDIUM - R-Tree operations may fail at runtime
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_COMPLIANCE_PROGRESS_REPORT.md
- Potential ambiguity markers detected:
  - L58: grep -c "TODO" src/core/rtree_index.cpp
  - L193: - May not make semantic sense for analytics workload
  - L307: **Mitigation**: May need to add helper function for vector deserialization
  - L345: - **Removed**: ~162 lines (TODO stubs, dead code)
  - L349: ### TODO Count
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_IMPLEMENTATION_AUDIT_RESULTS.md
- Potential ambiguity markers detected:
  - L97: 3. **TODO**: Extend `IndexInfo` structure to store index-specific configuration parameters
  - L98: 4. **TODO**: Integrate HNSW, BRIN, RTREE, COLUMNSTORE with proper parameter handling
  - L101: 1. **TODO**: Implement operator class registry for GIST and SPGIST
  - L102: 2. **TODO**: Add FULLTEXT as GIN-based index with text processing
  - L103: 3. **TODO**: Add index-specific parameters to catalog tables for persistence
  - L106: 1. **TODO**: Complete operator class implementations (box_ops, range_ops, etc.)
  - L107: 2. **TODO**: Add index configuration UI/API
  - L108: 3. **TODO**: Performance optimization and tuning
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_OPTIONAL_WORK_PLAN.md
- Potential ambiguity markers detected:
  - L1: # Index System Optional Work - Implementation Plan
  - L8: **Scope**: Complete P1 and P2 optional work to reach 100% index system maturity
  - L21: ### Target Status (Post-Optional Work)
  - L106: - ⚠️ TODO at line 198: "Flush to SSTable when memtable exceeds threshold"
  - L115: - ⚠️ Logged warning at line 710 (was TODO, now LOG_WARNING)
  - L240: // Search (should see entry)
  - L278: - **LSM-Tree Performance**: May affect existing behavior
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_SYSTEM_AGENT_TASKS.md
- Potential ambiguity markers detected:
  - L600: - ⚠️ Visibility map integration (TODO for runtime execution)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_SYSTEM_AUDIT_REPORT.md
- Potential ambiguity markers detected:
  - L11: **Status**: ✅ **100% COMPLETE** - All P0 critical + P1/P2 optional + final production-ready work completed
  - L23: **POST-OPTIONAL-WORK** (Evening): **9/11 production-ready, 11/11 MGA-compliant, 10/11 bytecode support, 11/11 test coverage** ✅✅
  - L41: **All P1/P2 Optional Work**: ✅ **COMPLETED**
  - L46: **TODO Count**: 13 → **0** ⬇️ (all targeted TODOs resolved)
  - L148: - ⚠️ One TODO found (line 616): "Implement in Phase 4" (minor feature)
  - L190: - ⚠️ One TODO (line 1460): "Implement more sophisticated heuristic" (optimization)
  - L335: - Resolved TODO at line 198 - replaced with proper documentation
  - L370: // TODO: Implement tree traversal and entry update
  - L374: // TODO: Implement full vacuum with TIP integration
  - L378: // TODO: Implement full GC traversal
  - L382: // TODO: Implement full tree traversal with chooseSubtree
  - L387: // TODO: Implement with actual page reads and area calculations
  - L392: // TODO: Implement full R* split algorithm
  - L395: // TODO: Implement full tree adjustment with split propagation
  - L398: // TODO: Implement with actual page operations
  - L401: // TODO: Implement full recursive search with MGA visibility
  - L406: **Production Readiness**: **QUESTIONABLE** - Depends on which implementation is actually used. If rtree_index.cpp is the executor integration point, R-Tree is **NOT PRODUCTION READY**. If rtree.cpp is used, it may be 80% ready.
  - L621: - If executor uses `rtree.cpp`, R-Tree may be 80% ready
  - L624: **Evidence**: See section 10 above for full TODO list.
  - L643: **Clarification Needed**: If these have specialized bytecode paths (e.g., for array indexing, vector search, analytical queries), the documentation should state:
  - ... 14 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_SYSTEM_COMPREHENSIVE_AUDIT.md
- Potential ambiguity markers detected:
  - L92: - **This violates pure Firebird MGA** - should use logical deletion
  - L340: - Should mark entries with xmax instead of physical removal
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_SYSTEM_REMEDIATION_PLAN.md
- Potential ambiguity markers detected:
  - L225: **Impact:** Unknown MGA compliance of underlying implementation
  - L255: **Impact:** Unknown performance characteristics
  - L363: 4. **R-Tree audit may reveal additional work**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/INDEX_VERIFICATION_REPORT_2025_11_06.md
- Potential ambiguity markers detected:
  - L31: - Has 6 TODO comments (not blockers):
  - L101: **Status**: ✗ INCOMPLETE - HAS TODO COMMENTS WITH UNIMPLEMENTED FUNCTIONS
  - L104: - **THREE TODO comments** indicating incomplete implementations:
  - L105: - "TODO: Deserialize vector and compute distance" (distance computation)
  - L106: - "TODO: Compute distance" (in search operations)
  - L107: - "TODO: Implement more sophisticated heuristic from HNSW paper (diversity-based)"
  - L129: - No TODO/NOT_IMPLEMENTED markers found
  - L145: - "TODO: Read metadata from catalog in Phase 7"
  - L146: - "TODO: In future phases, also scan from disk segments"
  - L147: - "TODO: Use TransactionManager for full TIP-based visibility"
  - L179: - Has TODO for logging
  - L228: - Has TODO markers for distance computation logic
  - L230: - May affect search quality/performance
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/MGA_CORRECTNESS_REVIEW.md
- Potential ambiguity markers detected:
  - L24: - **TODO is misleading**: Suggests PostgreSQL-style tuple manipulation (not needed in MGA)
  - L28: - **Reason**: Current implementation is correct, just inefficient for recursive locks
  - L48: ### The TODO Comments
  - L52: // TODO: Actually mark the tuple as aborted by setting HEAP_XMIN_ABORTED flag
  - L57: // TODO: Actually clear xmax by setting it to 0 and clearing HEAP_XMAX_VALID
  - L138: // TODO: Actually mark the tuple as aborted by setting HEAP_XMIN_ABORTED flag
  - L145: 1. The TODO suggests setting `HEAP_XMIN_ABORTED` (really `HEAP_XMIN_INVALID`)
  - L151: **The code is already correct without implementing the TODO.**
  - L153: ### What Should Happen Instead
  - L155: **Option 1: Remove the TODO** (Recommended)
  - L166: **Option 2: Set Hint Bits Eagerly (Optional Optimization)**
  - L168: // OPTIONAL: Set hint bits eagerly to avoid TIP lookup on first access
  - L173: **Recommendation**: **Option 1** - Remove the TODO. The current behavior (doing nothing) is correct MGA.
  - L184: SELECT * FROM test;  -- Should see row 1, not row 2
  - L198: **Current Implementation**: This behavior already works correctly via TIP.
  - L202: **If TODO is NOT implemented**:
  - L207: **If TODO IS implemented (setting flags explicitly)**:
  - L223: ### The TODO Comment
  - L226: // TODO: Implement proper per-proc-id lock tracking to support:
  - L235: #### Current Implementation Analysis
  - ... 11 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/PATH_TO_100_PERCENT.md
- Potential ambiguity markers detected:
  - L164: ### Performance Enhancements (Optional, for 100%)
  - L205: **Testing Status**: Existing tests should pass (backward compatible changes)
  - L265: // TODO: Support multi-page segments in future
  - L288: // TODO: In future phases, also scan from disk segments
  - L307: // TODO: Read metadata from catalog in Phase 7
  - L352: | **Main Gap** | No disk persistence (simple impl) | Missing 6 TODO items |
  - L485: ### TODO Summary
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/README.md
- Potential ambiguity markers detected:
  - L258: - 🟡 MEDIUM - Fix in current release (should fix soon)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/RTREE_MGA_AUDIT.md
- Potential ambiguity markers detected:
  - L21: **Risk Level:** HIGH - Current implementation violates MGA_RULES.md Rules 3, 5, 7, and 10
  - L57: - Entries should be marked with xmax, NOT physically removed
  - L126: - Concurrent transactions may see inconsistent data
  - L145: - If deleting transaction is aborted, entry should still be visible
  - L146: - If deleting transaction is active but not committed, entry should be visible to other transactions
  - L535: The stub is marked for API compatibility only, but should be updated:
  - L646: // Transaction T2: Search (should see entry)
  - L648: // Transaction T2: Search again (should still see entry)
  - L650: // Transaction T3: Search (should NOT see entry)
  - L698: - R-Tree may be used by other components
  - L704: - May impact search performance
  - L709: - May affect tree balance and split logic
  - L721: - xmax of aborted transaction should be ignored
  - L771: - **Should be elevated to Priority 1 (High)** due to critical violations
  - L843: The R-Tree implementation has critical MGA violations (physical deletion, incomplete visibility checks) that must be fixed before production use. The fixes are straightforward and estimated at 16 hours. The violations are identical to the GIN index, suggesting a pattern that should be addressed across all indexes.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SECURITY_AUDIT_REPORT.md
- Potential ambiguity markers detected:
  - L272: **Issue**: Permission cache has 60-second TTL but may not reflect immediate GRANT/REVOKE changes
  - L331: **Issue**: Materialized views may not enforce RLS policies
  - L348: -- View may contain all data (RLS not applied to materialization)
  - L382: 3. **CPU DoS**: Query planner may take infinite time
  - L441: - **std::random_device**: May be non-blocking (weak entropy)
  - L446: - Query optimizer may choose suboptimal plans
  - L518: - May leak database schema information
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SECURITY_AUDIT_SUMMARY.md
- Potential ambiguity markers detected:
  - L377: **Status**: Verified that the current implementation does not have this vulnerability.
  - L382: - Physical data population (TODO lines 8380-8384 in catalog_manager.cpp) not yet implemented
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SECURITY_SYSTEM_COMPREHENSIVE_AUDIT.md
- Potential ambiguity markers detected:
  - L268: | Operation | Line | TODO Comment | Vulnerability |
  - L445: // Test 1: Non-superuser attempts DROP USER (should fail)
  - L458: // Test 2: Non-owner attempts GRANT (should fail)
  - L473: // Test 3: Superuser performs all operations (should succeed)
  - L508: **Current Implementation** (`/home/user/ScratchBird/src/core/auth_provider.cpp:63-74`):
  - L547: **Current Implementation** (`/home/user/ScratchBird/src/core/password_hash.cpp:142-150`):
  - L650: // May reveal: "GEOS error: invalid geometry (libgeos version 3.8.0)"
  - L654: // May reveal: "PCRE2 error -3 at offset 6 (PCRE2 10.34)"
  - L676: **Recommendation**: Use OpenSSL for better entropy (optional)
  - L1028: **Post-Fix Status**: ✅ **PRODUCTION READY** (with optional enhancements)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SQL_PARSER_BYTECODE_AUDIT_REPORT.md
- Potential ambiguity markers detected:
  - L403: Based on grep analysis, the following extended opcodes may lack full executor implementation:
  - L406: - Some control flow opcodes (JUMP, LABEL, etc.) - may be compile-time only
  - L413: - MD5, SHA1, SHA256, SHA512 - may need verification
  - L416: - Complex geometric operations may be partial
  - L445: - Parser may support SELECT as insert source
  - L484: - Test file exists but coverage unknown
  - L502: ### 📝 TODO Items Found
  - L586: - Test current implementation
  - L619: 8. **Complete TODO Items**
  - L658: The system is **production-ready for basic SQL operations** but **requires additional work** for advanced features and complete SQL-92 compliance. The missing set operations are a significant gap that should be addressed before claiming full SQL support.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SQL_PARSER_BYTECODE_COMPREHENSIVE_AUDIT.md
- Potential ambiguity markers detected:
  - L57: - Column list (optional)
  - L168: - Column name list (optional)
  - L231: - WORK keyword (optional)
  - L235: - WORK keyword (optional)
  - L336: - Column list (optional)
  - L587: - **Some advanced features:** May be defined for future use
  - L616: - Pattern: LIKE (basic, TODO: wildcards)
  - L639: **Note:** These limitations are appropriate for expression indexes and CHECK constraints. Expansion may be needed for other use cases.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/SQL_STATEMENT_VERIFICATION_REPORT.md
- Potential ambiguity markers detected:
  - L334: 1. **It counts SAVEPOINT** which doesn't exist in code - should be 14
  - L335: 2. **It doesn't count ATTACH/DETACH TABLESPACE** which ARE implemented - should add 2 (16)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/STORAGE_ENGINE_AUDIT_REPORT.md
- Potential ambiguity markers detected:
  - L517: // TODO Phase 2 Enhancement: Implement soft delete by updating xmax field
  - L533: - ⚠️ TODO comment acknowledges this is "temporary measure"
  - L825: "Free space map changes may be lost.");
  - L838: - ⚠️ FSM changes may be lost on shutdown errors
  - L852: // TODO Phase 2 Enhancement: Implement soft delete by updating xmax field
  - L858: - ❌ Violates Firebird MGA principles (should use xmax marking)
  - L893: LOG_ERROR(STORAGE, "Failed to flush FSM! Free space map changes may be lost.");
  - L898: - ⚠️ FSM changes may be lost on abnormal shutdown
  - L934: - Should use xmax marking (soft delete)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/TECHNICAL_FINDINGS.md
- Potential ambiguity markers detected:
  - L63: - **Issue:** Default hash function may not distribute GPIDs well
  - L197: // TODO: findAllParallel should accept current_xid
  - L198: // TODO: findAnyParallel should accept current_xid
  - L200: - **Impact:** Parallel queries may return invisible tuples (ACID violation)
  - L275: - **Issue:** All in-memory, large indexes may not fit
  - L295: - **Comment:** "TODO: Implement ELSIF generation"
  - L299: - **Comment:** "TODO: Add IN/OUT/INOUT token support"
  - L320: // INT64_MAX + 1 = undefined behavior
  - L362: - **Comment:** "TODO: Implement proper LIKE with % and _ wildcards"
  - L534: std::optional<SequenceInfo> getSequence(const std::string& name) {
  - L630: throw std::runtime_error(message);  // Should use PSQL exception stack
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/bytecode_executor_audit_report.md
- Potential ambiguity markers detected:
  - L361: - ✅ Column name list (optional)
  - L456: - ✅ Called from INSERT when column not specified (Lines 3817-3828)
  - L677: - REVOKE executes but CASCADE may not fully work
  - L698: - Very complex expressions may have edge cases
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/archive/executor_dml_audit_report.md
- Potential ambiguity markers detected:
  - L28: - **TODO/FIXME markers:** 53 total (mostly in security/permission checks)
  - L269: **Quote:** Line 6139-6140: `// TODO: Parse and store argument expressions` followed by `error("Window function argument parsing not fully implemented");`
  - L562: - **TODO tracking:** 53 items (well-tracked)
  - L652: ## Appendix A: NOT_IMPLEMENTED / TODO Summary
  - L658: ### TODO Categories (53 total)
  - L686: | PSQL | 15+ | Unknown |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/00_EXECUTIVE_SUMMARY.md
- Potential ambiguity markers detected:
  - L65: ### 🟠 HIGH PRIORITY ISSUE #4: Deferred Work (105 TODO markers)
  - L152: **Finding**: **105 TODO/FIXME markers**
  - L262: 1. **MGA compliance scope** may uncover additional issues
  - L263: 2. **TOAST index integration** may impact performance
  - L269: 1. **TOAST GC edge cases** may require iteration
  - L270: 2. **Testing timeline** may extend
  - L338: 4. `04_DEFERRED_WORK_INVENTORY.md` - All TODO/FIXME markers
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/01_MGA_COMPLIANCE_AUDIT.md
- Potential ambiguity markers detected:
  - L586: **What Should Be Used**:
  - L626: **What Should Be Used**:
  - L726: // Returns a vector of TIDs (may be empty if key not found)
  - L859: - Should filter at index level using TIP lookups
  - L935: - Should use ONLY TIP-based visibility checks
  - L1172: **However:** Since `isSnapshotVisible()` and `getTransactionState()` are not found in the implementation, this method may not be implemented yet, or uses a different pattern.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/02_TOAST_IMPLEMENTATION_AUDIT.md
- Potential ambiguity markers detected:
  - L23: - ⚠️ **HIGH**: Indexes do NOT handle TOAST pointers (may index pointer bytes instead of actual values)
  - L283: **Current Implementation**: TOAST chunks use snapshot-based visibility via TupleHeader
  - L287: - Does NOT match Firebird MGA model where TOAST should be **TIP-independent**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/03_SQL_IDENTIFIER_AUDIT.md
- Potential ambiguity markers detected:
  - L270: - Data loss: Identifiers may be truncated unpredictably
  - L293: - Latin-1 extended (2-byte): 63 characters (WRONG - should support 128)
  - L294: - Chinese/Japanese (3-byte): 42 characters (WRONG - should support 128)
  - L295: - Emoji (4-byte): 31 characters (WRONG - should support 128)
  - L321: **Actual**: ⚠️ PASS in parser, but catalog write may corrupt if combined with other issues
  - L457: **Next Phase**: TODO/FIXME/DEFERRED Marker Inventory
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/AUDIT_CORRECTIONS_SUMMARY.md
- Potential ambiguity markers detected:
  - L13: All audit reports have been corrected to reflect the **market requirement** for **1:1 feature parity** with all 4 target databases (Firebird, MySQL, PostgreSQL, SQL Server). Previous assessments incorrectly used **engineering judgment** to mark features as "optional" or "out of scope."
  - L37: **Previous**: "90-95% complete - all core types implemented, specialized types optional"
  - L40: ### Missing Types (REQUIRED, not optional):
  - L81: ### Missing Index Features (REQUIRED, not optional):
  - L104: **Previous**: "25-30% complete - minimal viable set done, comprehensive optional"
  - L105: **Corrected**: "10-15% complete - comprehensive library REQUIRED, not optional"
  - L107: ### Missing Function Categories (REQUIRED, not optional):
  - L154: ### Missing SQL Features (REQUIRED, not optional):
  - L319: Without clarity on which features are MUST/SHOULD/NICE-TO-HAVE, the project cannot accurately plan for market release.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/COMPREHENSIVE_PRIORITY_AUDIT_2025-10-25.md
- Potential ambiguity markers detected:
  - L181: | 8 | sb_isql CLI | ❌ NOT AUDITED | ? | ? | ❓ Unknown |
  - L182: | 9 | Documentation | ❌ NOT AUDITED | ? | ? | ❓ Unknown |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/CORRECTED_COMPREHENSIVE_AUDIT_2025-10-25.md
- Potential ambiguity markers detected:
  - L195: **Missing/TODO Items**: Need to verify:
  - L407: **Estimated Gap**: Unknown until enumeration complete
  - L477: **Status**: ⚠️ UNKNOWN
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/FEATURE_PARITY_GAP_ANALYSIS.md
- Potential ambiguity markers detected:
  - L14: **Critical Understanding**: Previous audits incorrectly marked many features as "optional" or "out of scope" based on engineering judgment. The **market requirement** is that ScratchBird must achieve **1:1 feature parity** with all 4 target databases to be competitive.
  - L22: ### 1.1 Previously Marked "Optional" - Now REQUIRED
  - L68: 1. INET type (IPv4/IPv6 addresses with optional netmask)
  - L89: - tsvector/tsquery are fundamental to text search, not optional
  - L168: - Scientific/financial applications may require it
  - L327: The previous audit incorrectly treated "comprehensive function library" as optional. For 1:1 feature parity:
  - L391: - **Corrected**: **10-15% complete** (comprehensive library required, not optional)
  - L448: 8. **Subqueries** ⚠️ UNKNOWN
  - L536: - Foreign key constraints (may be missing)
  - L606: **Tier 2 - SHOULD HAVE (Competitive Parity)**:
  - L636: The previous audits used **engineering judgment** to mark features as "optional" or "out of scope" without considering **market requirements**. This gap analysis reveals:
  - L649: 1. Project owner clarifies which features are MUST/SHOULD/NICE-TO-HAVE
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/FUNCTION_COMPLETENESS_AUDIT.md
- Potential ambiguity markers detected:
  - L263: **A. Minimal Viable Set** (current implementation):
  - L367: NULLIF(status, 'unknown')
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/INDEX_TYPE_COMPLETENESS_AUDIT.md
- Potential ambiguity markers detected:
  - L432: - ⚠️ **Specialized index types** optional (Spatial, XML, Columnstore)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/PARSER_COVERAGE_AUDIT.md
- Potential ambiguity markers detected:
  - L450: 5. ✅ ALTER TABLE ADD/DROP COLUMN (~20-30 hours) - (optional, complex)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/PHASE_1_COMPLETION_AUDIT.md
- Potential ambiguity markers detected:
  - L21: - **Total TODO Comments Found**: 92 across 22 files
  - L132: **TODOs**: 2 non-blocking (optional enhancements)
  - L200: **TODOs**: 0 blocking (optional JSONB optimization deferred)
  - L224: ## TODO Analysis
  - L257: - ✅ `src/parser/parser.cpp` - 1 TODO (DEFER)
  - L350: ### 2. Optional Cleanup (Low Priority)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/QUERY_OPTIMIZATION_AUDIT.md
- Potential ambiguity markers detected:
  - L50: **Last Modified**: Unknown (planning document)
  - L253: **Should Be** (per spec):
  - L290: ### 3.2 What Should Exist for "Alpha Complete"
  - L351: - May result in Cartesian products or inefficient join sequences
  - L444: **Parser** (produces AST that optimizer should consume):
  - L452: **Indexes** (should be selected by optimizer):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/SCHEMA_STRUCTURE_AUDIT.md
- Potential ambiguity markers detected:
  - L397: - ⚠️ Advanced objects (triggers, procedures, sequences) optional for Alpha
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/TYPE_SYSTEM_COMPLETENESS_AUDIT.md
- Potential ambiguity markers detected:
  - L16: **CRITICAL CORRECTION**: Previous assessment incorrectly marked specialized types as "optional" based on engineering judgment. The **market requirement** is **1:1 feature parity** with all 4 target databases.
  - L22: - **Advanced Types**: ⚠️ Partial (ARRAY, COMPOSITE defined but may need runtime verification)
  - L25: **MISSING Types** ❌ (REQUIRED for 1:1 parity - NOT optional):
  - L70: | 42 | TIMESTAMP | Date + time (with optional timezone) | 8 | - |
  - L120: | **Decimal float** | DECFLOAT(16/34) | - | - | - | - | ❌ (optional) |
  - L122: | **Bit field** | - | BIT(n) | bit(n), varbit | bit | - | ❌ (optional) |
  - L127: - ⚠️ **Missing optional**: DECFLOAT, BIT(n), MEDIUMINT (covered by INT32)
  - L173: - ✅ **Timezone support**: TIMESTAMP has optional timezone flag (types.h:83-85)
  - L175: - ⚠️ **Missing optional**: YEAR (MySQL-specific), smalldatetime (legacy MSSQL)
  - L237: | DECFLOAT(16) | - | ❌ (optional) |
  - L238: | DECFLOAT(34) | - | ❌ (optional) |
  - L273: | BIT(n) | - | ❌ (optional) |
  - L326: | bit(n), varbit | - | ❌ (optional) |
  - L330: | int4range, int8range, numrange, tsrange, tstzrange, daterange | - | ❌ (optional) |
  - L427: - std::optional return values for fallible conversions
  - L547: - ❌ **Specialized types REQUIRED** (spatial, network, text search, range types) - NOT optional
  - L552: **CRITICAL CORRECTION**: Previous assessment incorrectly marked specialized types as "optional" or "out of scope" based on engineering judgment. The **market requirement** is 1:1 feature parity - if a type exists in ANY of the 4 target databases, users will expect it in ScratchBird.
  - L660: - inet (IPv4/IPv6 with optional netmask)
  - L701: **CRITICAL CORRECTION**: The original assessment of "90-95% complete" was based on **engineering judgment** that incorrectly marked specialized types as "optional." The **market requirement** for ScratchBird is **1:1 feature parity** with all 4 target databases.
  - L728: **Recommendation**: Implement spatial types and text search types as **CRITICAL priorities** for market competitiveness. These are not optional features - they are required for users to choose ScratchBird over existing databases.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/AUDIT_FIXES_MASTER_TODO.md
- Potential ambiguity markers detected:
  - L1: # ScratchBird Audit Fixes - Master TODO
  - L203: **Issue**: ~~Commit uses `sync()` which may be async~~ **AUDIT ERROR - ALREADY USES fsync()**
  - L211: - [x] Analyze platform compatibility - **Linux: ✅ Correct, macOS: Optional enhancement**
  - L223: - macOS F_FULLFSYNC optional enhancement (not needed for Alpha/educational scope)
  - L292: **Issue**: Overflow check happened AFTER calculation, causing undefined behavior
  - L293: **Impact**: Fixed - no more undefined behavior or heap corruption
  - L334: - **Before**: If `total_pages_ + num_pages` overflowed, undefined behavior occurred BEFORE the check
  - L440: **Issue**: ~~Calls `calculatePageChecksum()` which may not be defined/imported~~ **AUDIT ERROR - ALREADY CORRECT**
  - L468: - Auditor claimed function "may not be defined/imported" but it IS imported via clog.h → ondisk.h
  - L872: **Issue**: ~~Assignment may be move, causing use-after-move~~ **AUDIT ERROR - NO MOVE SEMANTICS AT LINE 824**
  - L897: - Auditor claimed "assignment may be move" at lines 822-843
  - L946: **Issue**: ~~Reads 2 bits but TransactionState enum may expand~~ **FALSE POSITIVE - Added forward compatibility protection**
  - L1304: - No undefined behavior in wraparound check ✅
  - L1383: - Audit may have confused ScratchBird with another database system
  - L1745: **Issue**: Sort happens AFTER assignment, binary search may fail
  - L1759: The audit claimed: "Sort happens AFTER assignment, binary search may fail"
  - L1777: // Optional read-only transaction filtering (lines 836-889)
  - L1797: 3. **Lines 836-889**: Optional read-only filtering (modifies array if filtering enabled)
  - L1803: - Read-only filtering may modify the array (removes some XIDs)
  - L1822: - Current implementation satisfies this requirement
  - ... 4 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/BTREE_COMPRESSION_IMPLEMENTATION_SUMMARY.md
- Potential ambiguity markers detected:
  - L311: - **Decision**: Compression is optional and automatic
  - L378: ### Current Implementation
  - L380: 2. **Fixed thresholds**: 8-byte/4-byte thresholds may not be optimal for all workloads
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/BTREE_COMPRESSION_TESTING_PLAN.md
- Potential ambiguity markers detected:
  - L68: make scratchbird_tests    # Should succeed after infrastructure fix
  - L258: - No crashes or undefined behavior
  - L304: ### Should Pass (Performance Goals)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/BTREE_COMPRESSION_TEST_RESULTS.md
- Potential ambiguity markers detected:
  - L211: ### Medium (Should Fix)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/BUFFER_POOL_EXHAUSTION_RESULTS.md
- Potential ambiguity markers detected:
  - L123: - Access 20 new pages (should evict cold pages)
  - L231: ### Issue 2: Sequential Access Pattern May Not Trigger Clock Sweep
  - L235: **Problem**: Tests 1 and 4 use sequential access, which may not stress the clock sweep algorithm
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/CI_CD_IMPLEMENTATION_SUMMARY.md
- Potential ambiguity markers detected:
  - L46: - Build: `-fsanitize=address,undefined,leak -fno-omit-frame-pointer -g -O1`
  - L128: - Warnings-as-errors: 8 critical checks (use-after-move, null-deref, dangling-handle, infinite-loop, sizeof-expression, undefined-memory, divide-zero, mt-unsafe)
  - L303: - UBSanitizer catches undefined behavior
  - L412: ### 2. Suppression Files May Need Tuning
  - L414: **Reason**: Some project-specific false positives may exist
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/COMPREHENSIVE_AUDIT_REPORT.md
- Potential ambiguity markers detected:
  - L65: // Current implementation (INCORRECT):
  - L221: The `sync()` may be a no-op or async flush, not guaranteed durable write.
  - L245: // Modifies mutable members - should NOT be const!
  - L254: - Potential optimizations based on const may break code
  - L280: - Undefined behavior
  - L335: **Issue**: `allocateClogPage()` calls `calculatePageChecksum()` but function is not defined/imported.
  - L363: next_page_num = page->btr_rightmost_child;  // May be 0!
  - L403: **Issue**: Free space calculation may be off by one item pointer.
  - L415: - insertTuple() may report success but corrupt page
  - L463: // But transaction state may not be persisted yet!
  - L512: If this is a move and active_xids is used later, it's undefined behavior.
  - L532: **Issue**: getTransactionState() reads 2 bits but TransactionState enum may require more.
  - L719: - Binary search may return incorrect results
  - L796: oldest_active_xid_ = new_oat;  // RACE: ProcArray may have changed!
  - L801: - VACUUM may delete tuples still visible to transactions
  - L939: // In strict mode, this should return false
  - L969: - New inserts may fail or overwrite data
  - L1381: - Line 555: Internal consistency - updateLru() should only be called with valid indices → **CONVERTED TO ASSERTION**
  - L1692: //   - PRIVATE helper methods may require caller to hold locks (documented per-method)
  - L1803: - Magic numbers that should be constants
  - ... 3 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/COMPREHENSIVE_TESTING_SUMMARY.md
- Potential ambiguity markers detected:
  - L482: 1. **Optional**: Suppress benign TSAN race on buffer content with annotations
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/CONCURRENT_PAGE_ACCESS_RESULTS.md
- Potential ambiguity markers detected:
  - L240: 4. **API Misuse**: Test may not be following correct transaction API patterns
  - L359: - Transaction commits may need retry on conflict
  - L360: - Snapshot acquisition may need retry on resource exhaustion
  - L392: - May be related to ProcArray backend limits or test infrastructure
  - L406: The failing tests may indicate:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/EXCEPTION_INJECTION_TEST_RESULTS.md
- Potential ambiguity markers detected:
  - L415: 2. ⏳ **Document backend limits** - ProcArray limit should be in operational guide
  - L467: 2. **CI/CD Integration**: Should add these tests to continuous integration
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FINAL_STATUS_BTREE_COMPRESSION_OCT17.md
- Potential ambiguity markers detected:
  - L396: - ✅ No undefined behavior
  - L423: - **Decision**: Compression is optional and automatic
  - L520: ### Current Implementation
  - L527: 2. **Fixed Thresholds**: 8-byte/4-byte thresholds may not be optimal for all workloads
  - L677: - CI/CD should build ALL tests
  - L756: make scratchbird_tests  # Should succeed after fixing main()
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.1_CRC32C_VERIFICATION_REPORT.md
- Potential ambiguity markers detected:
  - L173: The auditor may have:
  - L212: 2. **Understand Architecture**: Low-level functions may be composed by higher-level functions
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.2_ATOMIC_XID_VERIFICATION_REPORT.md
- Potential ambiguity markers detected:
  - L284: 6. ✅ **Updated Documentation**: This report + master TODO
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.3_BUFFER_POOL_LRU_VERIFICATION_REPORT.md
- Potential ambiguity markers detected:
  - L66: // This should never happen if callers are correct
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.4_HEAP_PAGE_MEMORY_LEAK_ANALYSIS.md
- Potential ambiguity markers detected:
  - L255: The current implementation is correct:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.4_HEAP_PAGE_MEMORY_LEAK_VERIFICATION_REPORT.md
- Potential ambiguity markers detected:
  - L307: **Solution**: Snapshots should be transaction-scoped.
  - L436: # Optional: Run under Valgrind to detect leaks
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.5_FSYNC_VERIFICATION_REPORT.md
- Potential ambiguity markers detected:
  - L10: **Classification**: AUDIT ERROR with Optional Enhancement Opportunity
  - L16: The audit report claimed that `Database::sync()` uses `sync()` which may be async and not guarantee durability. This is **INCORRECT**. The actual implementation at `src/core/database.cpp:994` uses `fsync(fd_)`, which DOES guarantee durability on Linux (the target platform).
  - L48: > The `sync()` may be a no-op or async flush, not guaranteed durable write.
  - L150: - `fsync()` may only flush to disk cache, not physical disk
  - L214: The audit should have traced `db_->sync(ctx)` to its implementation at `database.cpp:994` before claiming it uses `sync()`.
  - L243: ### Current Implementation Analysis
  - L257: | **macOS** | ⚠️ May cache | ⚠️ Not guaranteed | F_FULLFSYNC optional |
  - L323: **Recommendation**: **Option 3** - Keep current implementation. It's correct for the target platform.
  - L416: **Current Implementation**: ✅ COMPLIANT (on Linux via `fsync()`)
  - L423: **Current Implementation**: ✅ COMPLIANT (on Linux via `fsync()`)
  - L477: The audit incorrectly claimed that the code uses `sync()` which may be async. The actual implementation uses `fsync(fd_)` at `database.cpp:994`, which provides the required durability guarantees on Linux.
  - L495: ### Optional Enhancements (NOT REQUIRED)
  - L522: ### 3. Add Durability Tests (Optional)
  - L529: ### 4. Future Enhancement Tracking (Optional)
  - L552: - **Audit Claim**: Uses `sync()` which may be async
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/FIX_1.6_CONST_CORRECTNESS_VERIFICATION_REPORT.md
- Potential ambiguity markers detected:
  - L65: ### Current Implementation
  - L120: - Cache may be updated
  - L121: - LRU list may be reordered
  - L122: - Statistics may be incremented
  - L212: **Calls**: `getTransactionState()` which may update cache
  - L531: ### 3. Educate on C++ const Semantics (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/HELGRIND_AND_STRESS_TESTS_RESULTS.md
- Potential ambiguity markers detected:
  - L46: **Execution**: `valgrind --tool=helgrind ./tests/helgrind_races` (optional, tests also run standalone)
  - L402: 2. ⏳ **Document backend limits** - 100+ concurrent transactions may exceed ProcArray capacity
  - L458: 2. **ProcArray Sizing**: May need tuning for >100 concurrent transactions
  - L470: The ScratchBird database core is **production-ready** for high-concurrency workloads up to 100 threads. For workloads exceeding 100 concurrent transactions/snapshots, backend pool sizing should be reviewed.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_16_STATUS.md
- Potential ambiguity markers detected:
  - L17: ## Current Implementation Status: ✅ FULLY RESOLVED (2025-10-16)
  - L156: **Current Implementation (Full Firebird MGA)**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_17_STATUS.md
- Potential ambiguity markers detected:
  - L18: ## Current Implementation Status: ✅ IMPLEMENTED
  - L219: - May add helper function declarations
  - L229: - May need to update assertions
  - L317: **Breaking Changes**: None (backward compatible - compression is optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_18_STATUS.md
- Potential ambiguity markers detected:
  - L18: ## Current Implementation Status: NOT IMPLEMENTED
  - L284: - Small lists (< 10 TIDs) may not benefit
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_19_STATUS.md
- Potential ambiguity markers detected:
  - L18: ## Current Implementation Status: NOT IMPLEMENTED
  - L455: - **p50 latency**: May increase slightly (1-10ms due to batching wait)
  - L456: - **p95 latency**: Should improve (less contention)
  - L457: - **p99 latency**: Should improve significantly (no fsync storms)
  - L484: - Modified rollbackTransaction() (optional, ~30 lines changed)
  - L534: **Question**: Should rollback also use group commit?
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_2_20_STATUS.md
- Potential ambiguity markers detected:
  - L401: - Measure checkpoint time (should be < 25% of baseline)
  - L483: ### Current Implementation
  - L494: - Impact: May not be optimal for all workloads
  - L531: ## Next Steps (Optional Enhancements - Post-Beta)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_10_STATUS.md
- Potential ambiguity markers detected:
  - L297: Thread B: Store 101  ← Lost update! Should be 102
  - L360: The current implementation still uses `operator++` which uses sequentially consistent ordering:
  - L371: This would provide ~20-30% better performance for counter increments on x86-64, but the current implementation is correct and sufficient for Alpha.
  - L559: EXPECT_EQ(stats.hits, 100 * 1000);  // Should be exactly 100,000
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_1_STATUS.md
- Potential ambiguity markers detected:
  - L282: // XID should be on this page
  - L292: - TIP page reorganization (rare in current implementation)
  - L580: ## Next Steps (Optional - Future Enhancements)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_2_STATUS.md
- Potential ambiguity markers detected:
  - L65: - Internal helper method that should only be called with valid indices
  - L66: - Callers are internal and should provide valid inputs
  - L81: // This should never happen if callers are correct
  - L130: - Internal consistency checks (callers should provide valid inputs)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_3_STATUS.md
- Potential ambiguity markers detected:
  - L143: ### Current Implementation (Optimal)
  - L252: 1. **Pattern Matching Issues**: Automated tools may have flagged any XID-related code
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_5_STATUS.md
- Potential ambiguity markers detected:
  - L90: 1. **Prevent information leakage**: Uninitialized memory may contain remnants of previous data
  - L262: - ✅ Expected improvement: ~0.32% (very minor, may be within noise)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_7_STATUS.md
- Potential ambiguity markers detected:
  - L151: - No crash, no undefined behavior
  - L185: **RocksDB**: Optional Status details
  - L234: // Should NOT crash with nullptr ctx
  - L237: // Should still return error status
  - L247: // Should work correctly without crashing
  - L301: - The current implementation is correct
  - L305: ### 7.2 Optional Documentation Enhancement
  - L352: 2. **Context-sensitive analysis**: Tools may miss nullptr checks in macro bodies
  - L353: 3. **Pattern recognition**: Common idioms (optional error context) may be flagged
  - L365: 1. ✅ Optional error details (nullptr allowed)
  - L415: | **Testing Required**    | Optional (verify pattern)                        |
  - L416: | **Documentation**       | Optional comment for clarity                     |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_8_STATUS.md
- Potential ambiguity markers detected:
  - L25: - Subsequent operations may fail with out-of-bounds access
  - L108: "Correcting to buffer size (this may indicate corruption or config change).",
  - L128: - These serve different purposes and should both check page_size
  - L442: // Should fail on page_size check, not special area check
  - L486: // Re-read page and validate - should detect corruption
  - L634: - Subsequent operations may fail
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/ISSUE_3_9_STATUS.md
- Potential ambiguity markers detected:
  - L114: //   - PRIVATE helper methods may require caller to hold locks (documented per-method)
  - L416: //          May update tip_location_cache_ but doesn't require mutex_ (cache is best-effort).
  - L630: // Should complete without deadlocks
  - L646: // Should never deadlock with ProcArray operations
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/OCTOBER_17_2025_COMPLETION_SUMMARY.md
- Potential ambiguity markers detected:
  - L153: - Build flags: `-fsanitize=address,undefined,leak -fno-omit-frame-pointer -g -O1`
  - L154: - Detects: Memory errors, undefined behavior, leaks
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/SESSION_COMPLETION_SUMMARY_OCT17.md
- Potential ambiguity markers detected:
  - L273: - `make scratchbird_tests` (should succeed after Parser fix)
  - L316: - **Backward Compatible**: Compression is optional and automatic
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/TSAN_BUFFER_POOL_FINAL_RESULTS.md
- Potential ambiguity markers detected:
  - L215: **Should we fix it?**
  - L307: 1. **Optional**: Add per-page I/O lock to eliminate benign race (low priority)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/TSAN_BUFFER_POOL_TEST_RESULTS.md
- Potential ambiguity markers detected:
  - L207: When a page is pinned for I/O (flush), no other thread should be able to read/write the same page buffer until I/O completes.
  - L212: 3. Or: Database layer should acquire exclusive lock for I/O operations
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/UpdatedAuditWork/TSAN_LOCK_ORDERING_RESULTS.md
- Potential ambiguity markers detected:
  - L76: - This causes undefined behavior (mutex destroyed while in use)
  - L83: 2. **Test behavior**: Each test case creates a new Database, and GoogleTest may run test cases concurrently
  - L241: **Solution**: Each test should use a unique database path to avoid conflicts
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/ALPHA_COMPLETENESS_ASSESSMENT.md
- Potential ambiguity markers detected:
  - L22: 3. Migration State Management & Write Routing (unknown vs claimed 100%)
  - L110: - But coverage unknown (not measured in this audit)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/COMPREHENSIVE_CODE_AUDIT_2025-10-24.md
- Potential ambiguity markers detected:
  - L117: **Note**: Generic search for `updateTIDsAfterMigration` found 12 occurrences, suggesting methods may exist with different naming or namespacing. **REQUIRES DEEPER INVESTIGATION**.
  - L130: **Issue**: Compression header exists but implementation file is missing. This suggests compression may be header-only or incomplete.
  - L338: **Task 5.4.1 (Migration State Management)**: ⚠️ **UNKNOWN**
  - L430: | OFFLINE_TABLE_MIGRATION_TODOS.md | ⚠️ PARTIAL | Todo tracking |
  - L439: | SWEEP_INTEGRATION_PLAN.md | ❌ UNKNOWN | Not verified |
  - L613: 3. ⚠️ Sprint 4 State Management (unknown vs claimed 100%)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/CRITICAL_DISCREPANCIES_SUMMARY.md
- Potential ambiguity markers detected:
  - L44: - Task 5.4.1 (State Management): ⚠️ UNKNOWN - not verified
  - L70: 2. **Alpha release timeline** may be significantly underestimated
  - L127: 3. **Alpha release scope** may be incorrectly defined
  - L183: 1. **Methods may exist** with different naming/signatures
  - L184: 2. **Documentation may be inaccurate** (method names don't match code)
  - L186: 4. **Testing may be incomplete** (if methods don't exist as documented)
  - L215: - **Impact**: LOW - May be header-only implementation or incomplete
  - L231: | #2: Query Processing | 🔴 CRITICAL | Unknown (entire subsystem) | 100+ hours | P0 - CLARIFY NOW |
  - L232: | #3: Index TID Methods | ⚠️ MEDIUM | Unknown (may exist) | 0-30 hours | P1 - VERIFY SOON |
  - L233: | #4: Compression.cpp | 🟡 LOW | Unknown | 1-4 hours | P2 - LATER |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/faulty_2025-10-24/PLANNING_DOCUMENTS_REORGANIZATION.md
- Potential ambiguity markers detected:
  - L144: - Task 5.4.1 (State Management): ⚠️ UNKNOWN
  - L260: - OFFLINE_TABLE_MIGRATION_TODOS.md - Todo tracking
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_COMPLETION_COMPREHENSIVE_ANALYSIS.md
- Potential ambiguity markers detected:
  - L339: - ❌ RDB$DEPENDENCIES (object dependencies) - structure not defined
  - L384: ## 2. CURRENT IMPLEMENTATION STATUS
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_COMPLETION_DETAILED_TODO.md
- Potential ambiguity markers detected:
  - L1: # SCRATCHBIRD ALPHA COMPLETION - DETAILED TODO
  - L1191: 3. Check if PK index exists (it should - created automatically with PRIMARY KEY)
  - L1321: 4. Handle NULL values (NULL means unknown, constraint passes)
  - L1357: 4. Apply remaining WHERE filters (index may not cover all conditions)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_COMPLETION_DETAILED_TODO_PART2.md
- Potential ambiguity markers detected:
  - L1: # SCRATCHBIRD ALPHA COMPLETION - DETAILED TODO (PART 2)
  - L682: - `std::vector<std::string> column_names` (optional)
  - L809: 3. Handle cycle detection (optional): track visited tuples
  - L832: 2. Implement cycle detection: abort if same tuple appears twice (optional CYCLE clause)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_COMPLETION_DETAILED_TODO_PART3.md
- Potential ambiguity markers detected:
  - L1: # SCRATCHBIRD ALPHA COMPLETION - DETAILED TODO (PART 3)
  - L1137: 5. Which phases should be prioritized if timeline is compressed?
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_EXECUTIVE_SUMMARY.md
- Potential ambiguity markers detected:
  - L422: - ❌ Competitors may advance
  - L444: - ❌ May need adapter layer
  - L467: **Should Not Defer:**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_FINAL_COMPREHENSIVE_AUDIT.md
- Potential ambiguity markers detected:
  - L20: 1. ✅ TODO/FIXME/HACK comments and incomplete features
  - L206: 1. **GIN Index key_data buffer**: Magic number "54" should be sizeof() (gin_index.cpp:304)
  - L318: ## TODO/FIXME/HACK ANALYSIS
  - L323: - **TODO**: 33 occurrences
  - L331: 2. **storage_engine.cpp:812**: "TODO PHASE 2: This pointer direction is WRONG"
  - L383: #### Should Fix (High Priority):
  - L575: ## APPENDIX B: TODO SUMMARY
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_ISSUES_TRACKER.md
- Potential ambiguity markers detected:
  - L369: - Impact: Database corruption risk - pages marked as allocated/free may be incorrect after restart
  - L378: * Emergency sync may save some data even if FSM flush fails
  - L379: - Rationale: Can't throw exceptions in destructors (undefined behavior), but can log errors
  - L644: - Build flags: `-fsanitize=address,undefined,leak -fno-omit-frame-pointer -g -O1`
  - L744: ### Should Complete:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/ALPHA_PLANNING_INDEX.md
- Potential ambiguity markers detected:
  - L18: 4. Detailed TODO parts 1-3 - reference as needed for implementation
  - L61: - TODO/FIXME summary (47 markers)
  - L200: 3. Plan: All detailed TODO parts for implementation details
  - L214: → Find your phase in detailed TODO documents:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/AUDIT_SUMMARY_OCT_16_2025.md
- Potential ambiguity markers detected:
  - L25: - ✅ All TODO/FIXME markers cataloged
  - L38: | Low | 1 | Optional |
  - L151: ## TODO CATALOG: 47 MARKERS
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/INDEX_MGA_ALPHA_READINESS_SUMMARY.md
- Potential ambiguity markers detected:
  - L186: - OPTIONAL for ALPHA - most databases ship without this initially
  - L195: - MySQL: Uses REPEATABLE READ as default, SERIALIZABLE optional
  - L203: ### TASK 3.3: Optimize Visibility Checks ⏸️ OPTIONAL
  - L207: **Decision**: ⏸️ **DEFER TO BETA** - Optional performance optimization
  - L222: **Why Optional for ALPHA**:
  - L223: - Current implementation is CORRECT (all tests pass)
  - L226: - Should implement after ALPHA if profiling shows bottlenecks
  - L238: - Should be done after ALPHA release to establish baselines
  - L349: - Current implementation acceptable
  - L388: ### Optional for ALPHA (Can Defer)
  - L424: | TASK 3.2.2-3.2.3 | 14-19 hours | ⏸️ OPTIONAL | SERIALIZABLE not critical |
  - L425: | TASK 3.3 | 10-14 hours | ⏸️ OPTIONAL | Performance optimization |
  - L426: | TASK 3.4 | 8-12 hours | ⏸️ OPTIONAL | Benchmarking |
  - L465: **Features Still Deferred**: Blocked or Optional
  - L522: - ⏸️ TASKS 3.2.2, 3.3, 3.4: Optional for ALPHA (defer to BETA)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/INDEX_MGA_COMPLIANCE_ANALYSIS.md
- Potential ambiguity markers detected:
  - L60: | **Data Correctness** | 🟠 HIGH | May return invisible tuples | 4/4 indexes need visibility checks |
  - L94: - **IMPACT**: **HIGH** - May return uncommitted or old versions
  - L205: | **I-B1** | Phantom Reads Possible | 🔴 CRITICAL | Index may return tuples from uncommitted transactions |
  - L315: | **I-H2** | Deleted Tuple Visibility | 🔴 CRITICAL | May return TIDs for tuples deleted by uncommitted transactions |
  - L336: - May require database migration or rebuild indexes
  - L431: // ❌ Uncommitted entries may become permanently visible
  - L439: | **I-G1** | Pending List Visibility | 🔴 CRITICAL | Uncommitted entries in pending list may be visible after merge |
  - L441: | **I-G3** | Isolation Violations | 🔴 CRITICAL | Full-text searches may return results from uncommitted transactions |
  - L542: | **I-BM4** | False Positives | 🟠 HIGH | May return TIDs for tuples deleted and garbage collected |
  - L666: - Additional complexity: space partitions may need versioning
  - L1223: | B-Tree | ✅ Compatible (xmin/xmax optional) | ⚠️ Recommended | ✅ Yes | Optional |
  - L1243: 1. **Missing Visibility Filter**: Indexes don't check tuple visibility via snapshot - may return uncommitted data
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_audit/MGA_COMPLIANCE_REVIEW_TABLESPACE.md
- Potential ambiguity markers detected:
  - L401: -- Should be: [record1: ts1, AUTOEXTEND=OFF]
  - L405: -- Should be: [record1: ts1, AUTOEXTEND=OFF, MAXSIZE=1000]
  - L416: - No back version created (acceptable for catalog, but should update in-place)
  - L571: **Fix Priority**: **MEDIUM** (should fix before BETA, but not blocking ALPHA)
  - L573: - Should fix before BETA (production use)
  - L637: **Optional Enhancement**:
  - L667: -- ❌ Should be 1, will be ~201 (1 CREATE + 200 ALTER operations)
  - L674: -- ✅ Should be 1 (UPDATE in-place)
  - L753: - 🔧 Priority 3: Catalog garbage collection (2-3 hours) - optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/after_transaction_documentation_work.md
- Potential ambiguity markers detected:
  - L44: - All 15+ TODO markers updated
  - L71: - All TODO markers updated
  - L115: ### DOC-CRIT-004: Build Instructions May Be Outdated
  - L118: - **Impact:** New developers may not be able to build project
  - L129: - May need updated build targets documentation
  - L172: - Architecture diagram may not include new components
  - L184: 1. `TRANSACTION_MANAGEMENT_DESIGN.md` - May not reflect all Phase 3 features
  - L185: 2. `ISOLATION_LEVELS_DESIGN.md` - Should document implementation status
  - L212: - **Impact:** Test coverage for Phase 2/3 features unknown
  - L245: ### DOC-HIGH-006: TODO.md Doesn't Reflect Code Audit
  - L246: - **File:** `/docs/development/TODO.md`
  - L248: - **Impact:** TODO list doesn't match actual technical debt
  - L254: - May list completed items as TODO
  - L256: **Recommendation:** Regenerate TODO.md from audit report findings
  - L412: - May need "Implemented" sections added
  - L427: - **Age:** Unknown, needs review
  - L432: - **Age:** Unknown, needs review
  - L438: 5. `/docs/development/TODO.md` ⚠️
  - L440: - May list completed items
  - L460: - May be missing new dependencies
  - ... 9 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/after_transaction_work.md
- Potential ambiguity markers detected:
  - L78: - Note: Key extraction currently uses simplified tuple parsing (see TODO at line 1006-1009 for future enhancement with proper tuple deserializer)
  - L86: - **Impact:** Oldest transactions (which have done the most work) may be aborted repeatedly, causing starvation and poor performance.
  - L93: // TODO: Get XIDs from ProcArray
  - L150: // TODO: Implement connection lookup and rollback
  - L153: // TODO: Implement connection lookup and rollback
  - L156: // TODO: Implement connection lookup and termination
  - L169: - **Impact:** Cannot tune lock timeout for different workloads. 60 seconds may be too long for OLTP or too short for batch jobs.
  - L176: ### HIGH-002: TODO for Wait-for-Locks Configuration
  - L185: bool wait = true; // TODO: Get from ConnectionContext::getWaitForLocks()
  - L204: // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
  - L219: page_header_->btr_xmin = 0; // TODO: Integrate with transaction manager
  - L221: new_node->btn_xmin = 0; // TODO: Integrate with transaction manager
  - L229: - **Impact:** If MAX_SAFE_XID is set too high (close to UINT64_MAX), the overflow check may never trigger, causing XID wraparound.
  - L270: ### HIGH-007: Periodic Database Header Update May Cause I/O Spikes
  - L275: - **Impact:** Every 100th transaction incurs extra latency due to header write. May cause observable performance jitter.
  - L315: - **Impact:** After 1000 concurrent locks, system reverts to slower heap allocation. This threshold may be too low for high-concurrency workloads.
  - L333: - **Description:** `getTransactionState()` is called with `nullptr` ErrorContext in several places, but the function may dereference ctx.
  - L395: ## Medium Priority Issues (Should Fix)
  - L402: - **Impact:** Using arbitrary XID 100 may conflict with real transactions. Makes debugging harder.
  - L418: - **Impact:** If catalog layout changes, this hardcoded value may be wrong. Should be computed from actual catalog page count.
  - ... 24 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/gemini_audit.md
- Potential ambiguity markers detected:
  - L16: This audit identifies critical, high, and medium priority issues that should be addressed to move the project forward.
  - L22: - **`btree.cpp`**: The B-Tree implementation has placeholders for lock coupling (`// TODO(concurrency)`) but the implementation is not complete. The current locking strategy is coarse and may lead to deadlocks or race conditions, especially in `split_leaf_page` and `split_internal_page` where sibling pointers are updated without consistent locking.
  - L30: - **Key Comparison**: `BTree::compare_keys` is not defined in the provided code, which is a critical function for B-Tree operations. It's likely a placeholder that needs a proper, collation-aware implementation.
  - L31: - **Compression**: The B-Tree page compression logic is incomplete. `BTreePage::get_node` has a `// TODO` for decompression.
  - L42: - **`sblr/executor.cpp`**: The SBLR executor has several `// TODO` comments and incomplete implementations, particularly for aggregate functions (`AGG_SUM`, `AGG_AVG`, etc.) which are critical for analytical queries.
  - L58: - **`toast.cpp`**: The TOAST implementation relies on a heap scan (`deleteToastValueHeapScan`) if the index is not found. This is a reasonable fallback, but the primary path should always use the index for performance. The code should be hardened to ensure the index is always created and used.
  - L66: 1.  **Prioritize Concurrency**: The highest priority should be to complete the locking and concurrency model. This includes implementing a functional deadlock detector and completing the lock coupling in the B-Tree.
  - L67: 2.  **Complete B-Tree Implementation**: The B-Tree is a core component. The missing features, especially page merging and a robust key comparison function, should be implemented.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/gemini_docs_audit.md
- Potential ambiguity markers detected:
  - L32: The most significant issue is the pervasive confusion between future vision and current implementation.
  - L48: - **WAL Optionality:** The role of the Write-Ahead Log is described as both optional and required for durability in different places.
  - L83: *   This document must explicitly and concisely list what is **implemented and working**, what is **partially implemented**, and what is **not implemented**. It should be based directly on the findings of the latest code audit.
  - L87: *   Add a prominent disclaimer box at the top of all "future vision" documents (`ARCHITECTURE_GOALS.md`, all wire protocol specs, Y-Valve specs, etc.). The disclaimer should state: "**FUTURE VISION:** This document describes a long-term goal and is **NOT IMPLEMENTED** in the current Alpha version."
  - L111: 1.  **Adopt a "Living Documents" Approach:** Treat documentation as a product, not an artifact. It should evolve with the code. Deprecate and archive documents aggressively as they become obsolete.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/2025-11-01/older_work/type_cast_safety_audit.md
- Potential ambiguity markers detected:
  - L10: **Audit Reference:** MED-004 from TODO.md
  - L36: Found **4 const_cast instances** in production code. All are used to modify buffers passed as const, which technically violates const-correctness but may be necessary for specific use cases.
  - L150: **Priority:** HIGH - Should be fixed for correctness and maintainability.
  - L287: | **Aliasing** | May trigger strict aliasing issues | Explicitly allowed to alias |
  - L298: 3. **C++17 requirement:** Would require minimum C++17 (currently unknown)
  - L321: - Buffer pool may evict unpinned pages
  - L339: * - Unpinned pages: May be evicted at any time (no pointer validity guarantee)
  - L343: * - pinPage() may block if page is locked by another thread
  - L484: 4. **Create RAII wrapper for pinned pages (optional but recommended)**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/2025-09-10.REVIEW.md
- Potential ambiguity markers detected:
  - L22: **Issue:** The current path validation checks for `../` but may not be sufficient to prevent all forms of path traversal attacks, especially with more complex path encodings or symbolic links.
  - L77: **Recommendation:** Enforce a strict error handling policy. Every function that can fail should return a `Status` and set the `ErrorContext` when an error occurs. The caller should always check the returned status and propagate the error.
  - L105: The ScratchBird codebase is a solid foundation for a database engine. By addressing the issues outlined in this report, the development team can significantly improve the security, stability, and performance of the system. The recommendations provided should be prioritized based on their severity and potential impact on the overall system.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/AGENT_B_REVIEW_OF_AGENT_A_FIXES.md
- Potential ambiguity markers detected:
  - L9: Agent A has successfully addressed the critical issue I identified in my initial review. The page size validation fix is working correctly, and the performance degradation is understood and acceptable. However, I've identified a **new potential issue** that should be addressed.
  - L146: The new issue I've identified (special area reinitialization) is not critical but should be addressed to ensure complete correctness. Once this minor issue is fixed, the Extended Page Sizes feature will be fully production-ready.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/AGENT_B_REVIEW_STAGE_1_1_EXTENDED_PAGE_SIZES.md
- Potential ambiguity markers detected:
  - L213: The Stage 1.1 Extended Page Sizes implementation is well-executed and ready for integration. The identified issues are minor and can be addressed in follow-up commits. Agent C should focus testing efforts on the boundary conditions and regression scenarios outlined above.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/REVIEW_TEMPLATE.md
- Potential ambiguity markers detected:
  - L29: - Priority 1 (should-fix):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/agent_b_code_review_2025-09-08.md
- Potential ambiguity markers detected:
  - L29: *   There are several magic numbers in the code (e.g., `0x00010001` for the version). These should be defined as constants.
  - L37: *   The use of `std::mutex` is a good preparation for multi-threading, but the current implementation is not fully thread-safe. For example, the `stats_` member is not protected by the mutex.
  - L89: *   **`semantic_analyzer.cpp`**: The semantic analyzer is responsible for type checking and name resolution. The current implementation is basic and needs to be extended to support more complex SQL features.
  - L90: *   **`symbol_table.cpp`**: The symbol table is used to store information about tables, columns, and other database objects. The current implementation is basic and needs to be extended.
  - L96: *   **`bytecode_generator.cpp`**: The bytecode generator is responsible for converting the AST to SBLR bytecode. The current implementation is basic and needs to be extended to support more SQL features.
  - L97: *   **`executor.cpp`**: The SBLR executor is responsible for executing the SBLR bytecode. The current implementation is a placeholder and needs to be fully implemented.
  - L104: *   **Address the identified shortfalls:** The specific shortfalls identified in this report should be addressed in the next development cycle.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/agent_b_codebase_analysis_2025_09_16.md
- Potential ambiguity markers detected:
  - L19: The ScratchBird codebase is in a state of significant disarray. While individual components may be functional, the project as a whole suffers from a systemic disregard for its own established coding standards, a lack of automated quality enforcement, and inconsistent API design. These are not isolated problems but are pervasive throughout the reviewed components.
  - L77: *   As per `PROCESS_AND_AGENTS.md`, no code should be committed without a review from another agent. This process must be strictly enforced to catch logical errors and architectural inconsistencies that tools might miss.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/agent_b_heap_toast_review_2025-09-08.md
- Potential ambiguity markers detected:
  - L11: The Heap-TOAST integration is a significant and well-implemented feature. The design correctly separates the TOAST management from the core `HeapPage` logic, allowing for optional TOAST support. The automatic TOASTing and detoasting mechanisms are transparent to the user of `HeapPage`, which is a good design choice. The provided unit and integration tests demonstrate a good level of coverage for the core functionality.
  - L73: 1.  **TOAST Table Indexing:** `create_toast_table` has a `TODO` to create an index on `(chunk_id, chunk_seq)` for efficient retrieval. The current `delete_toast_value` and `read_toast_chunks` methods perform linear scans, which will be inefficient for large TOAST tables. This is a performance concern.
  - L74: 2.  **TOAST `next_value_id_` Initialization:** `ToastManager::initialize` has a `TODO` to read the max `value_id` from the TOAST table to set `next_value_id_`. Currently, it starts from a high number, which is a temporary workaround.
  - L75: 3.  **TOAST Chunk Cleanup on `insert_tuple` Failure:** `write_toast_chunks` has a `TODO` to clean up partially inserted chunks if an error occurs during insertion. This is a correctness/atomicity concern.
  - L82: The Heap-TOAST integration is largely **Implemented** and appears to be correct and robust for its current scope. The design is sound, and the code quality is good. The identified areas for improvement are primarily performance optimizations, missing features, or further integration steps that are consistent with the project's phased development approach. The current implementation provides a solid foundation for the TOAST/LOB feature.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_03_storage_engine_code_review_final.md
- Potential ambiguity markers detected:
  - L30: 4. **Hard-coded Values**: Magic numbers that should be configurable
  - L91: uint32_t current_xid_ = 100;  // Should be uint64_t per spec
  - L125: - Should be "Alpha 1.03" per AUTHORITATIVE_IMPLEMENTATION_PLAN.md
  - L130: - Should verify these don't conflict with existing codes
  - L219: The implementation provides a solid foundation for the database storage layer, but requires the fixes outlined above before it can be considered production-ready. The test suite is comprehensive for basic functionality but should be expanded to include stress testing and error injection scenarios.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_03_storage_engine_final_analysis.md
- Potential ambiguity markers detected:
  - L123: 2. **Hard-coded Values**: Page ranges (7-100) should be configurable
  - L141: With critical issues resolved, Agent C should now:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_04_transaction_foundation_final_review.md
- Potential ambiguity markers detected:
  - L106: 1. **Debug Output** - Commented fprintf statements should be removed
  - L108: 3. **Page Overflow** - TIP page chaining not implemented (noted as TODO)
  - L151: - May need to recalculate checksum after header update
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_04_transaction_foundation_fix_report.md
- Potential ambiguity markers detected:
  - L77: - May need additional investigation
  - L81: - May be a test assumption issue
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_04_transaction_foundation_review.md
- Potential ambiguity markers detected:
  - L44: - **No Page Chain Management**: Current implementation doesn't handle TIP page overflow
  - L55: **Problem**: Attempting to pin page 10 which may not exist in newly created database
  - L220: The fixes are straightforward - mainly changing how TIP pages are allocated and accessed. Once fixed, the implementation should be ready for integration.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_05_bytecode_generator_review.md
- Potential ambiguity markers detected:
  - L170: ## Minor Suggestions (Optional)
  - L188: 2. **JIT Compilation**: Optional performance optimization
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_05_semantic_analyzer_review.md
- Potential ambiguity markers detected:
  - L174: ## Minor Suggestions (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/alpha_1_05_sql_lexer_code_review.md
- Potential ambiguity markers detected:
  - L112: Current implementation doesn't handle nested `/* /* */ */` comments. This is fine for SQL but worth documenting.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/comprehensive_uuid_audit_2025_09_16.md
- Potential ambiguity markers detected:
  - L42: uint16_t column_id; // CRITICAL: Should be UuidV7Bytes (or ID)
  - L53: uint16_t column_id; // CRITICAL: Should be UuidV7Bytes (or ID)
  - L76: std::vector<uint16_t> column_ids; // CRITICAL: Should be std::vector<ID>
  - L89: uint16_t column_ids[16]; // CRITICAL: Should be an array of ID
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/firebase_review_2025_09_15.md
- Potential ambiguity markers detected:
  - L35: The most severe issue was found in the internal data structure used by the Catalog Manager to store table metadata. The public-facing APIs may have been updated, but the core `TableInfo` struct was not.
  - L44: uint32_t table_id; // CRITICAL: Should be UuidV7Bytes
  - L45: uint32_t schema_id; // CRITICAL: Should be UuidV7Bytes
  - L105: **Analysis:** This testing methodology is dangerous. It gives a false sense of security while completely missing the class of bugs related to handling large, complex, and non-sequential UUID values. The tests should be rewritten to use a `UuidV7Generator` to produce actual UUIDs for all identifiers.
  - L125: uint32_t table_id = 100; // PROBLEM: This should be a UuidV7Bytes object.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/phase_1_01_1_review.md
- Potential ambiguity markers detected:
  - L41: - File permissions set to 0644 (world-readable) - appropriate for Alpha but should be noted
  - L61: ### Priority 1 (should-fix):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/phase_1_01_2_review.md
- Potential ambiguity markers detected:
  - L127: ### Priority 1 (should-fix):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/audit_archive/legacy-reviews/phase_1_03_catalog_review.md
- Potential ambiguity markers detected:
  - L124: ### Priority 1 (should-fix):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/development_archive/COMPREHENSIVE_CODE_ANALYSIS_REPORT.md
- Potential ambiguity markers detected:
  - L162: - **TODO:** "Read max value_id from TOAST table to set next_value_id_"
  - L169: - **TODO:** "Clean up any chunks we already inserted"
  - L183: - **Should be:** `buffer_pool_->unpinPage(tip_header->next_tip_page, false, ctx);`
  - L213: - **Comment acknowledges:** "TODO: Implement a more efficient search"
  - L221: - **TODO:** "Implement split point calculation"
  - L226: - **TODO:** "Implement node removal"
  - L231: - **TODO:** "Insert the node into the sorted position"
  - L242: - **TODO:** "Rollback table record"
  - L280: - **Impact:** Reading uninitialized union → undefined behavior
  - L294: - **Impact:** Reading wrong union member → undefined behavior
  - L299: - **Impact:** Creates Token with undefined `type`, `location`, `length`, and union value
  - L319: - **Issue:** Default constructor undefined → uninitialized token
  - L340: 2. **Line 410: TODO**
  - L342: - **TODO:** "Parse AS alias if needed"
  - L367: - **Issue:** `popScope()` checks `scopes_.empty()` but should never be empty
  - L381: 2. Fix Token default constructor undefined behavior
  - L446: 1. **Line 140: TODO**
  - L448: - **TODO:** "Handle aliases"
  - L567: ## Part 5: TODO/FIXME Analysis
  - L678: - **Comment:** Function should implement B-tree insertion
  - ... 6 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/development_archive/COMPREHENSIVE_DOCUMENTATION_ANALYSIS_REPORT.md
- Potential ambiguity markers detected:
  - L50: #### Issue #1: Aspirational Architecture Presented as Current Implementation
  - L223: **Key Finding:** Only 14% of specifications accurately describe current implementation. 54% are future/aspirational documents presented as current specs.
  - L304: // TODO: Implement B-Tree insertion logic
  - L309: // TODO: Implement B-Tree removal logic
  - L395: **Impact:** Plans claim completion but cannot be verified. Work may be done but untested.
  - L487: - **Status**: UNRESOLVED - documented in TODO comments
  - L1096: - ❌ Aspirational features presented as current implementation (54% of specs)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/implementation/LSM_TREE_RANGE_SCAN_IMPLEMENTATION_2025-11-06.md
- Potential ambiguity markers detected:
  - L77: ├─→ Level 0 SSTables (all 4 files, may overlap)
  - L296: // TODO: Add proper logging
  - L462: - Scan should return 7 keys (skip deleted)
  - L466: - Scan should merge all levels
  - L470: - Scan should return newest version only
  - L474: - Transaction T2 scans (should not see T1's uncommitted changes)
  - L476: - Transaction T3 scans (should see T1's committed changes)
  - L480: - Scan should return only memtable version (newest)
  - L519: - LSM should be within 2x of B-Tree
  - L523: - Should skip 90%+ of irrelevant SSTables
  - L527: - Should scale O(N log K)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/implementation/LSM_TREE_RANGE_SCAN_PROGRESS_2025-11-06.md
- Potential ambiguity markers detected:
  - L51: - `Status::ERROR` doesn't exist (should be `Status::IO_ERROR`)
  - L310: - Should meet performance targets
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/implementation/views-implementation.md
- Potential ambiguity markers detected:
  - L296: -- Should insert into underlying table
  - L299: -- Should update underlying table
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/issues/ALPHA_1_2_REQUIREMENTS.md
- Potential ambiguity markers detected:
  - L44: - Some DDL commands may require being in their own transaction (exclusive access)
  - L122: - Background GC: dedicated thread (optional)
  - L530: - WAL (may add before SBLR)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/EXPLAIN_COMMAND_DESIGN.md
- Potential ambiguity markers detected:
  - L312: ss << prefix << "Unknown plan node type\n";
  - L413: The PlanNode classes already have `toString()` methods, but they may need enhancement for better EXPLAIN output.
  - L543: - Our cost values may differ (different cost model parameters)
  - L610: 1. `include/scratchbird/optimizer/explain_formatter.h` (optional - may use toString() instead)
  - L611: 2. `src/optimizer/explain_formatter.cpp` (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/FEATURE_PARITY_ROADMAP.md
- Potential ambiguity markers detected:
  - L25: | **Phase 2** | SHOULD HAVE | 800-1,200 | 5-7.5 months | 2.5-3.75 months | Features needed to compete with existing DBs |
  - L199: - [x] Evaluate HAVING clause parsing (execution deferred - noted in TODO) ✅ Done Oct 27
  - L210: - **Deliverable**: ✅ **DELIVERED** - `SELECT col, COUNT(*), SUM(val) FROM t GROUP BY col` fully executes (HAVING TODO noted)
  - L211: - **Note**: HAVING clause filtering marked as TODO in executeAggregate() - all infrastructure in place
  - L340: - ✅ Optional WHERE clause for both statements
  - L368: - [x] Add table alias support (AS keyword optional) ✅ Done Oct 25
  - L569: **Status**: Started October 27, 2025 → **~85% complete** (parser ✅, semantic ✅, planner ✅, bytecode ✅, executor ✅ - LIMIT TODO)
  - L608: **Phase 1.5 Status**: Parser ✅, Semantic ✅, Planner ✅, Bytecode ✅, **Executor TODO** (only execution remains!)
  - L709: - [ ] Performance optimization for JSONB binary format (optional) - Deferred to post-Alpha
  - L793: -- This query should work end-to-end:
  - L815: ## Phase 2: Competitive Parity (SHOULD HAVE)
  - L846: - [x] Implement POLYGON type ✅ Closed rings + optional holes
  - L1254: - **INET**: IPv4/IPv6 addresses with optional netmask
  - L1416: - Risk: R-tree implementation may not perform well
  - L1421: - Risk: Language design may be incompatible with targets
  - L1444: - [ ] Set up project board (TODO, In Progress, Done)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/JOIN_IMPLEMENTATION_COMPLETION.md
- Potential ambiguity markers detected:
  - L191: uint64_t num_pages = 100;  // TODO: Get from catalog
  - L192: uint64_t num_tuples = 10000;  // TODO: Get from catalog
  - L202: double qual_cost = 0.01;  // TODO: Calculate from WHERE clause
  - L236: core::ID left_table_id;  // TODO: Extract from left_path
  - L237: core::ID right_table_id;  // TODO: Extract from right_path
  - L455: // Unknown path type
  - L835: // Should extract 2 pairs
  - L847: // Test: Hash join should be cheaper for large tables
  - L848: // Test: Nested loop with index should be cheaper for small lookups
  - L901: - Small tables (< 1,000 rows): Nested loop may win
  - L902: - Large tables (> 10,000 rows): Hash join should win
  - L905: - With index on join key: Should beat hash join
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/JOIN_PLANNER_DESIGN.md
- Potential ambiguity markers detected:
  - L263: Assumes independence (may underestimate).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PHASE_1_CLEANUP_GUIDE.md
- Potential ambiguity markers detected:
  - L7: **Status**: Optional - Low Priority Enhancements
  - L15: Phase 1 is **100% functionally complete** with all 8 critical tasks delivered. The items in this guide are **optional cleanup tasks** that improve code quality, enable statistics persistence, and provide minor query plan optimizations. None are blocking for Phase 2.
  - L136: // TODO: Serialize MCVEntry array to mcv_data
  - L155: // TODO: Serialize HistogramBucket array to hist_data
  - L224: // TODO: Deserialize mcv_data to stats.most_common_values
  - L234: // TODO: Deserialize hist_data to stats.histogram
  - L339: Replace TODO at line 448:
  - L369: Replace TODO at line 473:
  - L447: // Right side should be constant or parameter
  - L802: These are **optional polish items** that improve code quality but don't add functionality. The audit confirmed Phase 1 is **100% functionally complete** without them.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PHASE_2_WAVE_2_STRATEGY.md
- Potential ambiguity markers detected:
  - L100: - Update SelectStmt to include optional WithClause
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PLANNER_INTEGRATION_DESIGN.md
- Potential ambiguity markers detected:
  - L91: Modify BytecodeGenerator to accept optional PlanNode:
  - L205: core::Database *database = nullptr);  // NEW: Optional database
  - L392: ### Task 1.3.6.4: Update Executor to Use Hints (Optional - Phase 2)
  - L430: -- Should use query planner and generate optimized bytecode
  - L438: -- Should choose IndexScan plan
  - L444: -- Without ANALYZE, should fallback to direct generation
  - L451: ### 1. Optional Database Parameter in BytecodeGenerator
  - L453: **Choice**: Make database parameter optional
  - L469: ### 3. Scan Hints as Optional Opcodes
  - L475: - Backward compatible (old executor ignores unknown opcodes)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/PSQL_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L50: Expression* default_value;  // Optional
  - L97: StringPool::StringId label;  // Optional
  - L102: StringPool::StringId label;  // Optional
  - L108: StringPool::StringId label;  // Optional
  - L109: Expression* when_condition;  // Optional (EXIT WHEN)
  - L113: Expression* return_value;  // Optional for procedures
  - L222: - IF/LOOP/WHILE statement body parsing is stubbed (recursive parsing TODO)
  - L457: - ELSIF clause generation stubbed (TODO in IF statement)
  - L458: - Jump patch offsets use placeholder logic (may need runtime refinement)
  - L849: - ✅ EXIT statement (with optional WHEN and labels)
  - L856: **Limitations in Current Implementation:**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/QUERY_PLANNER_DESIGN.md
- Potential ambiguity markers detected:
  - L647: Wait, this doesn't look right! The index scan should be cheaper.
  - L649: **Issue**: We're assuming 100 heap pages for 100 rows. Should be ~1 page.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/SELECTIVITY_ESTIMATION_DESIGN.md
- Potential ambiguity markers detected:
  - L17: - **Too high**: Optimizer may choose index scan when sequential scan is better
  - L18: - **Too low**: Optimizer may choose sequential scan when index scan is better
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/STATISTICS_COLLECTION_DESIGN.md
- Potential ambiguity markers detected:
  - L394: - Periodic background job (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_14_FULL_TEXT_SEARCH_PROJECT_PLAN.md
- Potential ambiguity markers detected:
  - L415: - [ ] **4.1.4** Add posting list compression (optional) - 3h
  - L817: ### Session 2: TBD
  - L819: - **Estimated Duration**: TBD
  - L839: - Performance benchmarks should be run at end of Phase 4
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_14_TEXT_SEARCH_TYPES_PLAN.md
- Potential ambiguity markers detected:
  - L134: std::vector<char> weights;           // Optional weights (A/B/C/D)
  - L279: // Arguments: config_name (optional), text
  - L312: // Arguments: config_name (optional), query_text
  - L554: ### External Libraries (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_15_PHASE_6_GIST_DESIGN.md
- Potential ambiguity markers detected:
  - L35: 3. **Operator Support**: Index should support:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_COMPLETE_IMPLEMENTATION_GUIDE.md
- Potential ambiguity markers detected:
  - L121: // TODO: Implement Expression::toString() for proper display
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_EXPRESSION_FILTERED_INDEXES_DESIGN.md
- Potential ambiguity markers detected:
  - L231: // Parse optional WHERE clause
  - L587: Even if index is used, may need to re-check predicate at query time.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_MGA_COMPLIANCE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L654: // Scan with xid1 - should see only first version
  - L703: // TX2: Re-query (should NOT see new row - snapshot isolation)
  - L705: // Should not include 'new@test.com' even though it matches predicate
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_MGA_COMPLIANCE_IMPLEMENTATION_PLAN_REVISED.md
- Potential ambiguity markers detected:
  - L41: - ✅ ADDED: Audit logging for monitoring (optional)
  - L114: #### ✅ 2.1 Optional Debug Logging (1 hour actual, 2-3 hours estimated)
  - L480: 1. CREATE INDEX with rollback (no entries should be visible)
  - L502: // Index search should return NO results
  - L586: ### 2. Audit Logging is Optional ✅
  - L604: - Index may have entries from aborted transactions
  - L616: 3. ⏳ Visibility checks in index scanning (Phase 3.1) - TODO
  - L617: 4. ⏳ Testing of rollback behavior (Phase 4) - TODO
  - L619: ### IMPORTANT (Should have for production)
  - L620: 1. ⏳ GC integration (Phase 2.3) - TODO
  - L621: 2. ⏳ markDeleted() method (Phase 3.2) - TODO
  - L622: 3. ⏳ Visibility-aware range scans (Phase 3.3) - TODO
  - L625: 1. ⏳ Debug logging (Phase 2.1) - TODO
  - L626: 2. ⏳ Statistics tracking (Phase 2.2) - TODO
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_MGA_PHASE_3_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L81: - Set to 0 in `BTreePage::add_node()` (line 76: `// TODO: Integrate with transaction manager`)
  - L156: new_node->btn_xmin = 0; // TODO: Integrate with transaction manager
  - L203: **Challenge**: Page splits create new internal nodes. What xid should they use?
  - L538: 2. Search with snapshot xid = 99 (should not see)
  - L539: 3. Search with snapshot xid = 101 (should see)
  - L541: 5. Search with snapshot xid = 199 (should see)
  - L542: 6. Search with snapshot xid = 201 (should not see)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/TASK_17_PHASE_6_13_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L224: // TODO: Call appropriate index insert method
  - L354: When implementing deferred phases, developers should:
  - L412: - Performance optimization (Phase 12) - may reveal design issues
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/WAVE_2_AGENT_SPECS.md
- Potential ambiguity markers detected:
  - L60: std::vector<std::string> column_aliases;  // Optional column aliases
  - L75: // Update SelectStmt to include optional WITH clause
  - L107: // Optional column aliases: (col1, col2, ...)
  - L157: // Parse optional WITH clause first
  - L1300: - IN/EXISTS should optimize to semi-joins when possible
  - L1389: FOR_EACH_STATEMENT  // Optional, can defer
  - L1519: // Optional parentheses: procedure_name()
  - L1526: // Optional semicolon
  - L2009: // Execute INSERT (should fire trigger)
  - L2036: // For UPDATE, both OLD and NEW should be available
  - L2061: // Both should fire on INSERT
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/ALPHA_1_2_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L364: - TODO CRIT-002
  - L475: ### Task 2.4: Update All TODO Markers (Week 7, 5 days) ✅
  - L478: 1. **Audit All TODO Markers (1 day)** ✅
  - L479: - Find all "TODO(concurrency): Get proc_id from thread-local storage"
  - L533: - [x] All 15+ TODO markers updated
  - L555: - TODO CRIT-004
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/MGA_PROPER_INDEX_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L61: **Problem**: Current implementation was incompatible with new offset-based back versioning.
  - L120: - Indexes may become stale/corrupted on UPDATE operations
  - L130: This document provides a detailed implementation plan to correctly implement **Firebird-style Multi-Generational Architecture (MGA) with back versioning** in ScratchBird. The current implementation uses **PostgreSQL-style forward versioning**, which causes index bloat and write amplification—the exact problems that MGA was designed to solve.
  - L134: **Current Implementation** (PostgreSQL-style):
  - L243: // Start at REQUESTED tuple (may not be newest)
  - L261: **Problem**: With forward pointers, must start at HEAD of chain (newest) and traverse forward to find visible version. Current code may not always start at newest.
  - L429: // TODO: Implement cross-page back version allocation
  - L1083: * - Index writes per UPDATE (should be ~0)
  - L1084: * - Heap writes per UPDATE (should be constant)
  - L1134: - May need `is_back_version` flag
  - L1161: - May need helper functions for back version allocation
  - L1216: ### 6.2 Migration Tool (Optional)
  - L1392: ⚠️ **SHOULD HAVE**:
  - L1406: 1. **Index write reduction**: ≥70% reduction vs current implementation
  - L1562: All TODO PHASE 2 comments added to mark locations needing algorithmic fixes.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/PHASE_2_COMPLETE.md
- Potential ambiguity markers detected:
  - L20: - ✅ All 15+ locking TODO markers resolved - locking operational
  - L92: **TODO Markers Resolved:** 15+
  - L97: - Removed TODO markers for proc_id retrieval
  - L123: // TODO: Get proc_id from thread-local storage when ConnectionContext is implemented
  - L421: - No more TODO markers for proc_id
  - L485: **Before Phase 2:** "15+ locations have locking disabled with TODO markers" (CURRENT_STATUS.md line 115)
  - L487: **After Phase 2:** All TODO markers resolved, locking enabled throughout codebase.
  - L539: - ✅ **Enabled locking throughout the codebase** (15+ TODO markers resolved)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT0_BUG_FIX_COMPLETE.md
- Potential ambiguity markers detected:
  - L280: // TODO PHASE 2: This pointer direction is WRONG (forward not back)
  - L288: **80% write amplification** is a HUGE performance hit. This bug would have made ScratchBird 5x SLOWER than it should be for UPDATE-heavy workloads on indexed tables.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT1_FOUNDATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L119: // ... find free page (should succeed immediately)
  - L223: - Allocates preallocated pages (should succeed)
  - L224: - Next allocation should fail with INVALID_ARGUMENT
  - L250: LOG_WARNING(CATALOG, "This will cause dangling TOAST references - table may be unusable");
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT3_ONLINE_MIGRATION_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L893: // 2. Final copy of remaining dirty pages (should be < 100 pages)
  - L998: LOG_WARNING(STORAGE, "Unknown index type %d, skipping TID update",
  - L1006: **Problem**: Queries started before SWAP may still reference source tablespace.
  - L1218: - Considerations: May need write pause for convergence
  - L1224: - Considerations: May benefit from parallel copy threads (future enhancement)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/SPRINT3_SUMMARY.md
- Potential ambiguity markers detected:
  - L76: **Problem**: Where should new INSERTs go during migration?
  - L90: **Problem**: High write load may prevent migration from completing.
  - L96: - Optional: Brief write pause (< 1 second) to force convergence
  - L109: - Copy final dirty pages (should be < 100)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_CONTEXT_VARIABLES_DESIGN.md
- Potential ambiguity markers detected:
  - L53: - **Optional precision**: `CURRENT_TIME(2)` for milliseconds
  - L254: ) const -> Result<std::optional<std::string>, Status>;
  - L260: std::optional<std::string> value
  - L480: return 0;  // No visible version (should not happen)
  - L666: mutable std::optional<Date> current_date_cache_;
  - L1417: [[nodiscard]] auto getCurrentTriggerTable() const -> std::optional<std::string> {
  - L1561: std::optional<uint8_t> precision;
  - L1621: std::optional<uint8_t> precision = expr->getPrecision();
  - L1706: mutable std::optional<Date> current_date_cache_;
  - L1707: mutable std::optional<TimeWithTZ> current_time_cache_;
  - L1708: mutable std::optional<TimestampWithTZ> current_timestamp_cache_;
  - L1950: // UUID should be preserved
  - L2158: **Risk**: Users may try to use trigger-only variables outside triggers.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_ROW_IDENTITY_AND_TRANSACTION_VISIBILITY.md
- Potential ambiguity markers detected:
  - L44: 3. **Optional surfacing**: If user declares UUID column as identity, use row UUID instead of generating new one
  - L117: #### 3. Optional User-Defined UUID Column
  - L535: #### Task A.5: Optional UUID Identity Column (5 hours)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_ROW_IDENTITY_ENHANCED.md
- Potential ambiguity markers detected:
  - L236: - Optional surfacing as identity column
  - L369: // WHERE clause (optional)
  - L374: // WITH clause (optional)
  - L532: - Optional source delete
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/INDEX_MGA_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L131: **What Was NOT Needed** (and should NOT be added):
  - L166: - [ ] **1.2.6**: Add MVCC tests - ⏸️ **OPTIONAL** (lower priority, existing code works)
  - L396: **Current Implementation**:
  - L501: **Note on TID Conversion**: Roaring bitmaps use 32-bit values. ScratchBird TIDs are 64-bit (page_id << 32 | item_id). Current implementation truncates to lower 32 bits. For production with large databases, consider using a sequential TID mapping or 64-bit bitmap library.
  - L591: - **Note**: Full end-to-end index cleanup testing requires table metadata integration (see cleanIndexes TODO)
  - L603: - **TODO**: Full implementation requires table metadata integration to:
  - L609: - **Pseudocode documented** in TODO comments for future implementation
  - L635: - This integration work is documented with TODO and pseudocode for future implementation
  - L689: - **Status**: Deferred as optional enhancement
  - L690: - **Reason**: Current implementation is sufficient for Alpha; full transaction-aware merge adds complexity
  - L745: - ⏸️ Merge preserves MVCC semantics (deferred as optional enhancement)
  - L1150: - Test file exists but may need CMake integration verification
  - L1404: **Future Optimizations** (optional):
  - L1429: After careful analysis, **this task should NOT be implemented** as specified. The task is based on PostgreSQL MVCC assumptions that don't apply to ScratchBird's Firebird MGA architecture.
  - L1487: **Priority**: N/A (Snapshot isolation already complete, SERIALIZABLE optional)
  - L1502: **Subtasks 3.2.2-3.2.3 are OPTIONAL for ALPHA**:
  - L1520: - **Optional for ALPHA**: Can fallback to REPEATABLE READ
  - L1545: **Status**: ⏸️ **DEFER TO BETA** - Optional performance optimization
  - L1546: **Reason**: Current implementation is CORRECT and PERFORMANT for ALPHA
  - L1566: - Current implementation is FUNCTIONAL and CORRECT
  - ... 8 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/MVCC_VS_MGA_CODE_REVIEW.md
- Potential ambiguity markers detected:
  - L58: // Insert new tuple version on the new page  // ← WRONG! Should create BACK version!
  - L395: -- Index scan should fail (TID changed, index points to old location)
  - L405: -- Index scan should work (TID unchanged, index still valid)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/OFFLINE_TABLE_MIGRATION_DESIGN.md
- Potential ambiguity markers detected:
  - L25: 9. [Session Todo Lists](#session-todo-lists)
  - L242: - `SET` (may need to add)
  - L354: * @param progress_callback Optional callback for progress updates
  - L404: // Page not found in mapping - should not happen
  - L469: // TODO: Implement lock manager integration
  - L515: // TODO: Scan heap to count pages
  - L532: // TODO: Scan all heap pages in source tablespace
  - L554: // TODO: Update index TIDs
  - L572: // TODO: Rollback changes
  - L593: // TODO: Free all old pages using freePageGlobal()
  - L597: // TODO: Implement lock release
  - L633: - TID positions may change if GPID changes
  - L674: "Unknown index type");
  - L721: // Periodic checkpoint (optional - adds complexity)
  - L865: SELECT COUNT(*) FROM t1;  -- Should be 1000
  - L911: ALTER TABLE t1 SET TABLESPACE full_disk_ts;  -- Should fail gracefully
  - L951: ## Session Todo Lists
  - L953: See [OFFLINE_TABLE_MIGRATION_TODOS.md](./OFFLINE_TABLE_MIGRATION_TODOS.md) for detailed session-by-session todo lists.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/OFFLINE_TABLE_MIGRATION_TODOS.md
- Potential ambiguity markers detected:
  - L1: # Offline Table Migration - Session Todo Lists
  - L243: - [ ] Parser accepts optional `ONLINE` clause
  - L313: * @param progress_callback Optional progress callback (pages_copied, total_pages)
  - L481: // TODO: Implement lock manager integration
  - L519: // TODO: Count total pages and indexes
  - L525: // TODO: Implement in Task 2.5
  - L528: // TODO: Implement in Session 3
  - L540: // TODO: Implement in Task 2.7
  - L556: - May need to scan from first page until end marker
  - L571: // TODO: Implement page reading logic
  - L592: // TODO: Rollback - free all allocated pages
  - L647: // TODO: Rollback
  - L782: ALTER TABLE t1 SET TABLESPACE ts2;  -- Should copy pages
  - L897: "Unknown index type");
  - L913: // TODO: Get index root page from IndexInfo
  - L914: // TODO: Scan all leaf pages
  - L915: // TODO: For each TID in leaf:
  - L918: // TODO: Mark pages dirty
  - L925: **Note**: This may require deep integration with index subsystem. Consider:
  - L1101: SELECT * FROM t1 WHERE id = 500;  -- Should use index
  - ... 8 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_1_PARSER_TEST.md
- Potential ambiguity markers detected:
  - L17: Successfully implemented parser support for the `ALTER TABLE ... SET TABLESPACE` statement with optional ONLINE clause.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_3_PROGRESS_TRACKING.md
- Potential ambiguity markers detected:
  - L108: - Added `progress_callback` parameter (optional, defaults to `nullptr`)
  - L170: // Check if we should log progress (every 5 seconds)
  - L455: assert(pages_copied >= last_progress); // Progress should be monotonic
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_4_BATCH_PROCESSING.md
- Potential ambiguity markers detected:
  - L162: // Even tiny tables should use at least this many pages per batch
  - L449: ❌ **Index Inconsistency**: Indexes may reference old TIDs, new TIDs, or mix (corrupt)
  - L553: // Verify: Should process in 1 batch (5 pages < MIN_BATCH_SIZE_PAGES)
  - L554: // Log should show: "batch size: 5 pages"
  - L563: // Verify: Should process in ~50-page batches (500 / 10 = 50)
  - L564: // Log should show: "batch size: 50 pages"
  - L573: // Verify: Should process in 1000-page batches (MAX_BATCH_SIZE_PAGES)
  - L574: // Log should show: "batch size: 1000 pages, ~7.8 MB/batch"
  - L575: // Log should show: "100 batches"
  - L579: // Verify: Memory usage should NOT exceed ~20 MB (batch + overhead)
  - L608: Account for compressed pages (may be smaller than 8KB):
  - L616: Large TOAST values may span multiple pages, adjust batch size:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE4_TASK4_1_5_INDEX_TID_UPDATE.md
- Potential ambiguity markers detected:
  - L24: While the current implementation is a **STUB** (does not perform actual TID updates), it establishes the complete infrastructure and integration points for full implementation.
  - L847: // Migrate (should skip index update)
  - L850: // Log should show: "No indexes found, skipping index TID update"
  - L860: // Log should show:
  - L873: // Log should show:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_1_HEAP_PAGE_MIGRATION.md
- Potential ambiguity markers detected:
  - L61: The current implementation has STUB code in these areas:
  - L220: // Note: FSM is owned by PageManager, may need to add API
  - L286: - Migration should succeed immediately (no pages to copy)
  - L331: // May need to scan all pages in tablespace file instead
  - L374: VACUUM; -- May free some pages
  - L389: The current implementation simulates page copying with `std::this_thread::sleep_for()`. We need to implement real page copying that:
  - L872: - Multiple tuples may span multiple pages
  - L1047: - Solution: Rollback pages, but indexes may have stale TIDs
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_FULL_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L583: // (May need to add BTree::open() API)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_1_1_HEAP_PAGE_ENUMERATION.md
- Potential ambiguity markers detected:
  - L168: **Impact**: Cannot precisely filter pages by table. Current implementation returns **all HEAP pages in the tablespace**, not just pages for the target table.
  - L172: 2. Primary tablespace (0) may have multiple tables, but this is rare in optimized deployments
  - L225: The following test cases should be validated in future integration testing:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_1_2_PAGE_COPYING_TID_REMAPPING.md
- Potential ambiguity markers detected:
  - L196: // TODO Phase 5.1.3: Implement rollback logic
  - L286: The following test cases should be validated in future integration testing:
  - L362: ### 1. No Rollback Implementation (TODO Phase 5.1.3)
  - L372: // TODO Phase 5.1.3: Implement rollback logic
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_1_3_TOAST_HANDLING.md
- Potential ambiguity markers detected:
  - L49: LOG_WARNING("This will cause dangling TOAST references - table may be unusable after migration");
  - L250: -- Expected: Warnings logged (has_toast may be true for schema), migration succeeds
  - L298: | **Error Handling** | ⚠️ Warning only (optional fatal) | ✅ Fatal error if TOAST migration fails |
  - L380: **Lesson**: Some users may prefer fatal errors over warnings.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_1_4_TRANSACTION_ROLLBACK.md
- Potential ambiguity markers detected:
  - L369: The following test cases should be validated in future integration testing:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_2_BTREE_TID_UPDATES.md
- Potential ambiguity markers detected:
  - L416: **Issue**: Prefix-compressed keys may become invalid after TID changes.
  - L420: **Impact**: Queries may fail if compressed key relies on old TID ordering.
  - L511: - Migration should be atomic (all-or-nothing)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_3_OTHER_INDEX_TID_UPDATES.md
- Potential ambiguity markers detected:
  - L222: -- - Overflow chains may exist if buckets are full
  - L394: **Issue**: Multiple directory entries may point to same bucket page.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE5_TASK5_4_ONLINE_MIGRATION_ANALYSIS.md
- Potential ambiguity markers detected:
  - L19: After thorough analysis, **ONLINE migration should remain deferred to post-BETA**. The current OFFLINE migration implementation (Tasks 5.1-5.3) provides a solid, production-ready foundation that covers 90-95% of use cases. ONLINE migration requires significant additional infrastructure and introduces substantial complexity and risk.
  - L137: - May need to block writes temporarily (defeats purpose of ONLINE)
  - L202: ## Why ONLINE Migration Should Be Deferred
  - L476: **ONLINE migration should remain deferred to post-BETA**. The current OFFLINE migration implementation provides a solid, production-ready solution that covers 90-95% of use cases with minimal risk. Implementing ONLINE migration now would require 80-100 hours of complex, high-risk work for a feature that benefits < 5% of users.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/PHASE7_COMPLETE_SCOPE.md
- Potential ambiguity markers detected:
  - L19: **User Directive**: "Fully implement TABLESPACE support with all possible optional enhancements implemented."
  - L24: 3. **Optional enhancements** (nice to have for ALPHA)
  - L29: ## Current Implementation Status
  - L814: | Feature Area | Priority | Estimated Hours | Optional? |
  - L833: **SHOULD HAVE** (High value):
  - L850: - Compression (12-16 hours) - SHOULD
  - L851: - Encryption (14-18 hours) - SHOULD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT2_IMPLEMENTATION_PROGRESS.md
- Potential ambiguity markers detected:
  - L311: ### Option 2: Test Current Implementation (3-4 hours)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT2_INDEX_TOAST_ANALYSIS.md
- Potential ambiguity markers detected:
  - L47: src/core/bitmap_index.cpp           # ? Bitmap (unknown status)
  - L70: #### Current Implementation Review
  - L190: #### Current Implementation Review
  - L261: #### Current Implementation Review
  - L321: - May just be a specialized GIN index with text tokenization
  - L336: ### Current Implementation (Sprint 1)
  - L346: LOG_WARNING(CATALOG, "This will cause dangling TOAST references - table may be unusable");
  - L590: Sprint 2 scope has been **fully analyzed and documented**. The work required is substantial (26-41 hours) and should be executed in phases based on priority.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT4_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L20: **Recommendation**: Due to the complexity and scope (30-37 hours), Sprint 4 implementation should be executed in a dedicated focused session or across multiple sessions, with careful testing at each stage.
  - L71: default: return "UNKNOWN";
  - L315: // TODO: Implement bloom filter allocation
  - L332: // TODO: Write to pg_table_migrations catalog table
  - L418: // TODO: Persist to catalog
  - L453: // TODO: Persist to catalog
  - L488: // TODO: Trigger rollback
  - L489: // TODO: Deallocate target pages
  - L490: // TODO: Free migration state resources
  - L610: // Check if TID might be in filter (may have false positives)
  - L664: * Determines which tablespace a TID should be fetched from during ONLINE migration.
  - L958: // TODO: Implement per-table cache invalidation
  - L1121: // (This should already exist from Sprint 0 bug fix)
  - L1303: **Note**: This plan assumes Sprint 0 (MGA bug fix) was completed correctly. If not, UPDATE routing may need additional work.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT5_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L418: // Check if worker should stop
  - L465: // 2. Pin target page (new, should be empty)
  - L604: std::optional<ID> executeMigration(
  - L613: std::optional<ID> CatalogManager::executeMigration(
  - L929: **Add**: Optional write pause for forced convergence
  - L946: // Note: This is a last resort, should rarely be needed
  - L950: // TODO: Implement table-level exclusive lock
  - L952: LOG_WARN("Write pause not yet implemented - migration may fail");
  - L1011: // TODO: Table-level exclusive lock
  - L1014: // Step 2: Copy final dirty pages (should be very few)
  - L1021: LOG_WARN("High dirty page count during swap ({} pages) - may exceed 100ms target",
  - L1030: // TODO: Release lock
  - L1039: // TODO: Release lock
  - L1047: // TODO: Release lock, rollback catalog
  - L1052: // TODO: Release table lock
  - L1089: // TODO: Transaction support
  - L1111: // TODO: Commit
  - L1160: // TODO: Get index implementation and call batchUpdateTIDs()
  - L1327: // TODO: FSM update (if implemented)
  - L1428: // TODO: Persist migration history to disk
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT5_SUMMARY.md
- Potential ambiguity markers detected:
  - L128: - Optional: Brief write pause for forced convergence (future)
  - L173: - Copy final dirty pages (should be < 100)
  - L200: // TODO: Table-level exclusive lock
  - L268: // TODO: FSM update
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SPRINT7_PHASE7_PREPARATION.md
- Potential ambiguity markers detected:
  - L11: **Priority**: MEDIUM (Optional for ALPHA, recommended for production)
  - L94: ### Feature Area 3: Compression (12-16 hours) - SHOULD HAVE
  - L123: ### Feature Area 4: Encryption (14-18 hours) - SHOULD HAVE
  - L389: 1. **Compression Performance**: May exceed 10% overhead target
  - L392: 2. **Encryption Key Management**: External KMS may be unavailable
  - L395: 3. **Backup/Restore Integrity**: Backup may not capture all data
  - L400: 1. **Statistics Overhead**: Tracking may impact performance
  - L432: 2. **Prioritize features** (confirm MUST HAVE vs SHOULD HAVE)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/SWEEP_INTEGRATION_PLAN.md
- Potential ambiguity markers detected:
  - L178: // TODO: Need table_id in PageHeader or use catalog lookup
  - L194: // TODO: Need CatalogManager::getIndexesForTable()
  - L201: // TODO: Need way to get index object from index_id
  - L227: // TODO: Add index GC stats to GCStatistics struct
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_COMPLETE_IMPLEMENTATION_ROADMAP.md
- Potential ambiguity markers detected:
  - L171: > **The Problem with Current Implementation**:
  - L717: - Handle tuple not found (may have moved)
  - L773: - Trade-off: Some tuples may remain in source tablespace until next UPDATE
  - L871: - Otherwise, migration may never complete (high write load)
  - L912: - Queries started before swap may still reference source tablespace
  - L1114: ### PHASE 7: Advanced Features (TBD)
  - L1116: **Estimated Effort**: TBD (depends on scope)
  - L1147: **NOTE**: Phase 7 scope to be determined based on user requirements and ALPHA goals.
  - L1156: - Blocking: Phase 6 (attach/detach may need autoextend)
  - L1165: - Blocking: Phase 7 (advanced features may depend on attach/detach)
  - L1175: - ~~Task 5.4.1-5.4.3 (State management, visibility, routing) SHOULD complete before 5.4.4-5.4.7 (copy/swap)~~ ✅ **COMPLETE**
  - L1355: ### Sprint 8: Advanced Features (TBD)
  - L1356: 1. Phase 7: TBD based on requirements
  - L1364: **Risk**: Complex changes to core query path may introduce bugs or performance regression
  - L1374: **Risk**: High write load may prevent migration convergence
  - L1392: **Risk**: Low usage index types may have incomplete implementation
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L366: - **FSM loading deferred to Task 1.3.5** (noted in TODO comment)
  - L375: - **FSM flushing deferred to Task 1.3.5** (noted in TODO comment)
  - L705: - ✅ TODO: Delete file from filesystem (deferred)
  - L706: - ✅ TODO: Invalidate catalog record (deferred to compaction)
  - L771: - [x] Can create tablespace with all optional parameters ✅ COMPLETE
  - L815: - ✅ TODO: Update TablespaceHeader on disk (requires PageManager API)
  - L821: - ✅ TODO: Update TablespaceHeader.tablespace_name on disk (requires PageManager API)
  - L876: - [x] TODO: Changes persisted to TablespaceHeader (deferred - requires PageManager API)
  - L905: - Updated `CreateTableStmt` AST node with optional tablespace field (StringPool::StringId)
  - L906: - Added optional TABLESPACE parsing in `Parser::parseCreateTable()`
  - L1012: **Assignee**: TBD
  - L1186: - Performance test: Preallocate 10GB, measure time (should be < 1 second with fallocate)
  - L1228: **Assignee**: TBD
  - L1235: - [OFFLINE_TABLE_MIGRATION_TODOS.md](./OFFLINE_TABLE_MIGRATION_TODOS.md) - Detailed session-by-session todo lists
  - L1284: - ✅ Updated `moveTableToTablespace()` signature with optional progress_callback parameter
  - L1372: - Stress test: Migrate table under concurrent read load (should block readers)
  - L1380: **Assignee**: TBD
  - L1405: - Hash: Rehash into new tablespace (may need full rebuild)
  - L1571: - TODO: Rollback logic (deallocate copied pages) deferred to Task 5.1.3
  - L1605: - **No rollback on failure**: Allocated pages not deallocated on error (TODO: Task 5.1.3)
  - ... 2 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_ROADMAP_SUMMARY.md
- Potential ambiguity markers detected:
  - L68: - Phase 7: Advanced Features (TBD)
  - L172: ### Sprint 8: Advanced Features (TBD)
  - L183: **NOTE**: Scope to be determined based on ALPHA requirements.
  - L199: | **Sprint 8** (Advanced) | TBD | MEDIUM | ⏸️ NOT STARTED |
  - L230: **Risk**: Complex changes to core query path may introduce bugs
  - L240: **Risk**: High write load may prevent migration convergence
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/ALPHA_ADVANCED_SECURITY_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L314: ### **Phase 3.2: Performance Foundations** (OPTIONAL - Lower Priority Now)
  - L318: #### 3.4.1: Permission Result Caching (OPTIONAL - 3-5 hours)
  - L391: #### 3.1.3: Bulk Permission Checks (Optional, 2-3 hours)
  - L416: - `src/sblr/executor.cpp` - Optional usage in multi-table queries (+20 lines)
  - L928: ### Optional Enhancements
  - L938: **Total Optional:** 13-20 hours (2-3 days)
  - L979: ### Week 4 (Optional): Policy-Based Access Control & Polish
  - L1015: - May need to extend for special functions (current_user_id, etc.)
  - L1055: - Test cross-tenant queries (should fail)
  - L1117: - Q: Should we compile policies to native code for performance?
  - L1154: **Estimated Total Time:** 40-58 hours (critical + optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/DATA_TYPE_COMPLETION_PLAN_2025-11-06.md
- Potential ambiguity markers detected:
  - L68: // TODO: Implement CHECK constraint evaluation
  - L161: INSERT INTO table VALUES (-5::positive_int);  -- Should fail
  - L610: **Risk**: Adding COMPOSITE/VARIANT may complicate TypedValue
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/DDL_MODIFICATIONS_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L309: // TODO: Free heap pages used by table data
  - L362: // TODO: Free index pages
  - L376: The current implementation provides basic CASCADE support for indexes. For complete dependency tracking:
  - L450: - Missing token types: `KW_CASCADE`, `KW_RESTRICT` may need to be added to lexer
  - L507: // Verify it's gone (should throw)
  - L513: // Drop non-existent table with IF EXISTS (should succeed)
  - L516: // Drop without IF EXISTS (should fail)
  - L526: // RESTRICT should fail
  - L529: // CASCADE should succeed
  - L542: // Table should still exist
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/EXTRACT_FUNCTION_COMPREHENSIVE_PLAN.md
- Potential ambiguity markers detected:
  - L56: **Storage:** int64_t (microseconds since epoch) + optional timezone
  - L222: **Storage:** template<T> Range { optional<T> lower, optional<T> upper, BoundType, empty }
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/FK_PHASE_C_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L156: ### Optional (Can Defer):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_COMPLETION_ROADMAP.md
- Potential ambiguity markers detected:
  - L14: This document provides a comprehensive roadmap for completing all 11 index implementations to 100%. Based on the audit findings and current implementation state, the total estimated effort is **350-450 hours** of development work.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_CORRECTION_ACTION_PLAN_2025-11-06.md
- Potential ambiguity markers detected:
  - L35: | TODO comments | MEDIUM | 13 | Future work markers |
  - L175: - L0 SSTables: All may overlap, need merge
  - L338: - Compare scan speed vs B-Tree (should be within 2x)
  - L828: -- Should exist at: /var/lib/scratchbird/ts_custom/idx_test.gin
  - L893: -> std::optional<double> {
  - L913: auto VectorValue::euclideanDistance(const VectorValue& other) const -> std::optional<double> {
  - L921: auto VectorValue::cosineSimilarity(const VectorValue& other) const -> std::optional<double> {
  - L929: auto VectorValue::manhattanDistance(const VectorValue& other) const -> std::optional<double> {
  - L937: auto VectorValue::dotProduct(const VectorValue& other) const -> std::optional<double> {
  - L946: - ✅ No TODO comments (verified in code)
  - L1007: // Range scan with T2 should NOT see key1
  - L1018: // Range scan with T3 SHOULD see key1
  - L1030: - [ ] No new TODO or NOT_IMPLEMENTED markers
  - L1045: - Update "Current Implementation Status"
  - L1062: - ✅ All TODO comments addressed or moved to future work
  - L1099: - [ ] All critical TODO comments addressed
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_IMPLEMENTATION_FIX_PLAN.md
- Potential ambiguity markers detected:
  - L72: - Open all SSTables that may contain keys in range
  - L525: **Should be**:
  - L679: // TODO: Implement cosine distance
  - L683: // TODO: Implement Manhattan distance
  - L687: // TODO: Implement dot product distance
  - L793: **Phase 5: SIMD Optimization (Optional)** (2-3 hours)
  - L829: - ✅ No TODO comments in computeDistance()
  - L832: - ✅ (Optional) SIMD optimizations provide 4-8x speedup
  - L976: - [ ] All distance metrics work (cosine, Manhattan, dot product currently TODO)
  - L981: - [ ] All TODO comments addressed
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/INDEX_INTEGRATION_IMPLEMENTATION_SUMMARY.md
- Potential ambiguity markers detected:
  - L158: **Integration Point**: Inside `EXTENDED_OPCODE` case, before "Unknown extended opcode" error
  - L415: ### Optional Enhancements
  - L438: **Total Optional Work**: ~1,050 lines, 15-20 hours
  - L461: - B-Tree `markDeleted()` method signature (may need to use `remove()` if `markDeleted()` doesn't exist)
  - L462: - Index `open()` static method signatures may vary
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/MISSING_FUNCTIONS_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L166: ### Optional (Can Defer)
  - L334: 6. Optional: Add XMLCONCAT, XMLFOREST
  - L394: ### External Libraries (Optional)
  - L414: **Decision**: Start with standalone implementations, add library support as optional enhancement.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/PERMISSION_CACHE_OPTIMIZATION_PHASE3_2_3.md
- Potential ambiguity markers detected:
  - L89: std::optional<bool> lookup(const CacheKey& key);
  - L151: std::optional<bool> PermissionCache::lookup(const CacheKey& key) {
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/QUERY_PLAN_SECURITY_INTEGRATION.md
- Potential ambiguity markers detected:
  - L549: - Q: When should security cache be invalidated?
  - L577: - ✅ No false positives (grant access when should deny)
  - L578: - ✅ No false negatives (deny access when should grant)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/QUERY_PLAN_SECURITY_PHASE3_2.md
- Potential ambiguity markers detected:
  - L170: // (may require SELECT on indexed columns - Phase 3.3)
  - L368: SELECT * FROM employees;  -- Should fail at plan time
  - L376: SELECT * FROM employees;  -- Should succeed
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SECURITY_PHASE3_3_COLUMN_LEVEL_PLAN.md
- Potential ambiguity markers detected:
  - L300: // NEW: Optional column list for column-level permissions
  - L382: **Extended GRANT opcode** (same 0xCA, with optional columns):
  - L391: [2 bytes] column_count (optional, if has_columns flag set)
  - L392: [string]  column_name_1 (optional)
  - L393: [string]  column_name_2 (optional)
  - L612: // User in role should have permission
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SECURITY_PHASE3_4_RLS_PLAN.md
- Potential ambiguity markers detected:
  - L186: - USING expression is required, WITH CHECK is optional
  - L404: 3. **Optimization**: RLS predicates should be pushed down to scan nodes
  - L503: **Problem**: Multiple policies require OR logic, but query optimizer may not handle complex ORs well
  - L761: **Risk**: RLS predicate injection may break query optimizer assumptions
  - L769: **Risk**: RLS overhead may be unacceptable for high-throughput queries
  - L777: **Risk**: Parsing stored policy expressions back into AST may be error-prone
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L21: 5. [Phase 4: Optional Features (Future)](#phase-4-optional-features-future)
  - L153: const optional<string>& new_password,
  - L154: const optional<ID>& new_default_schema,
  - L1792: ### 3.6: Column-Level Privileges (Optional, 20-30 hours)
  - L1832: - ⚠️ Column-level privileges (optional)
  - L1843: ## Phase 4: Optional Features (Future)
  - L2005: **Optional**:
  - L2063: | Phase 4: Optional Features | 100-150 | 3-4 | LOW | ⏳ Pending |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SEQUENCE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L176: // Optional parameters (nullptr if not specified)
  - L322: - Optional parameters with defaults
  - L343: - Sequence functions (sequence name + optional value)
  - L383: auto alterSequence(const ID& sequence_id, const std::optional<int64_t>& increment_by,
  - L384: const std::optional<int64_t>& min_value, const std::optional<int64_t>& max_value,
  - L385: const std::optional<int64_t>& restart, const std::optional<int64_t>& cache_size,
  - L386: const std::optional<bool>& cycle, ErrorContext* ctx = nullptr) -> Status;
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/SQL_OBJECT_PERMISSIONS_DESIGN.md
- Potential ambiguity markers detected:
  - L724: - Q: Should triggers use definer or invoker rights?
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/TRUNCATE_IMPLEMENTATION_CODE.md
- Potential ambiguity markers detected:
  - L21: // Optional TABLE keyword
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/TRUNCATE_TABLE_ASYNC_IMPLEMENTATION.md
- Potential ambiguity markers detected:
  - L266: // Optional TABLE keyword
  - L383: // TODO: Return job_id to user (need result mechanism)
  - L412: -- Legacy syntax support (TABLE keyword optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/TRUNCATE_TABLE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L80: KW_TRUNCATE,  // (check if exists, may need to add)
  - L99: // Optional TABLE keyword
  - L338: 4. TRUNCATE with foreign keys (future: should fail with RESTRICT)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/VIEWS_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L64: char column_names[4096]; // Comma-separated column names (optional)
  - L108: **Challenge**: Dropping a table used by a view should either:
  - L144: // CHECK and OPTION may already exist
  - L186: std::vector<StringPool::StringId> column_names_;  // Optional column list
  - L250: // Optional column list: (col1, col2, ...)
  - L283: // Optional WITH CHECK OPTION
  - L383: std::string query_text = "<query_text>";  // TODO: serialize SELECT to string
  - L412: std::vector<std::string> column_names;  // Optional explicit columns
  - L500: // TODO: Check for dependent views if CASCADE is false
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/code_quality_remediation_plan_2025_09_16.md
- Potential ambiguity markers detected:
  - L48: - [ ] The style should be based on a standard format like "LLVM" or "Google" and then customized to match the project's specific standards.
  - L82: - [ ] This should automatically rename many of the `snake_case` functions to `camelCase`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/column_uuid_migration_plan.md
- Potential ambiguity markers detected:
  - L68: - [ ] In `TEST_F(CatalogManagerTest, CreateAndGetTable)`, when defining the `columns` vector, the `column_id` field should be left in its default-initialized state, as the `create_table` function is now responsible for generating it.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive/legacy-plans/remediation_plan_2025_09_15.md
- Potential ambiguity markers detected:
  - L60: - [x] Change the line `uint32_t table_id = 100;` to `UuidV7Bytes table_id = uuid_gen.Generate();`. You may need to add a comment indicating that `uuid_gen` is a `UuidV7Generator` instance.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_CLI_TOOLS_AND_VIEWS_COMPLETION_PLAN.md
- Potential ambiguity markers detected:
  - L49: - Connection pooling (optional)
  - L245: std::cerr << "Unknown command: " << cmd << "\n";
  - L421: - [  ] Fix mode implemented (optional)
  - L633: - [  ] Interactive mode (optional)
  - L671: Should:
  - L837: - Attempt to update non-updatable view (should error)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_CONSTRAINTS_AND_ENGINE_COMMANDS_PLAN.md
- Potential ambiguity markers detected:
  - L373: 3. Parse optional sequence options (START WITH, INCREMENT BY)
  - L380: - Multiple IDENTITY columns (should error - only one per table)
  - L416: - INSERT with explicit value for ALWAYS (should error)
  - L632: 4. Parse optional FROM clause
  - L633: 5. Parse optional LIKE pattern
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_MASTER_COMPLETION_TRACKER.md
- Potential ambiguity markers detected:
  - L379: ### Current Implementation Status
  - L429: ### Current Implementation Status (Updated November 22, 2025)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA1_PSQL_TRIGGERS_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L435: - Bytecode execution is already ~90% stubbed, so integration should be straightforward
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L576: ### Phase G: Security Polish (Optional, 12-20 hours)
  - L599: **Phase G**: 1 week (12-20 hours, optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/CTE_IMPLEMENTATION_STATUS.md
- Potential ambiguity markers detected:
  - L29: - ✅ Supports optional column aliases: `WITH cte (col1, col2) AS ...`
  - L66: - ✅ CTEDefinition stores name, query, and optional column aliases
  - L201: ### 3. Nested CTE References (Unknown Status)
  - L264: ### Current Implementation
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/LSM_TREE_COMPLETION_PLAN.md
- Potential ambiguity markers detected:
  - L49: - ⚠️ Compression (Snappy/Zstd) - OPTIONAL
  - L50: - ⚠️ Parallel compaction - OPTIONAL
  - L62: | **Compression** | ⚠️ Not implemented (OPTIONAL) | Larger disk usage | **P3 - LOW** |
  - L64: | **Parallel Compaction** | ❌ Not implemented (OPTIONAL) | Slower compaction under heavy write load | **P3 - LOW** |
  - L65: | **Block Cache** | ❌ Not implemented (OPTIONAL) | More disk I/O for hot data | **P3 - LOW** |
  - L242: // TODO: Implement k-way merge
  - L243: // TODO: Implement garbage collection based on OIT
  - L244: // TODO: Implement atomic SSTable replacement
  - L349: ### Phase 2: Optional Enhancements (Priority 3)
  - L632: **Week 3 (Optional Enhancements):**
  - L772: - ✅ Compression support (optional but recommended)
  - L788: 1. **Should we implement compression immediately or defer to Phase 2?**
  - L791: 2. **Should we keep simple LSMTree for backward compatibility?**
  - L794: 3. **What Bloom filter false positive rate should we target?**
  - L797: 4. **Should parallel compaction be P2 or P3?**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/MERGE_AND_RETURNING_IMPLEMENTATION.md
- Potential ambiguity markers detected:
  - L83: Expression* condition;  // Optional additional condition
  - L193: // Parse optional column list
  - L568: // Fetch the tuple (may need to get from old version for DELETE)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/MGA_COMPLIANCE_FIX_PLAN.md
- Potential ambiguity markers detected:
  - L108: **Start Date**: TBD
  - L109: **Target Completion**: TBD
  - L1152: # Should return 0 results
  - L1191: # 1. Check for Snapshot contamination (should be 0)
  - L1195: # 2. Check for isSnapshotVisible (should be 0)
  - L1198: # 3. Check for TIP usage (should be many)
  - L1201: # 4. Check for isVersionVisible (should be many)
  - L1421: **Target Completion**: TBD (150-220 hours estimated)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/PHASE_3_REVISED_TASKS.md
- Potential ambiguity markers detected:
  - L200: * @param data Input data (may be TOAST pointer or inline data)
  - L335: -- Should return row (index has actual value)
  - L339: -- Should show actual text, NOT 18-byte pointer
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/PSQL_IMPLEMENTATION_STATUS.md
- Potential ambiguity markers detected:
  - L40: - ✅ Proper error handling for undefined variables
  - L78: - ✅ Optional loop labels
  - L101: - ✅ Optional loop labels
  - L114: **Status:** Fully implemented (may need minor refinement for body parsing)
  - L123: - ✅ Optional WHEN condition
  - L210: **Note:** Currently throws C++ exception, not PSQL exception system. Should integrate with exception stack.
  - L383: 3. **Error Handling:** Proper error messages for undefined variables, etc.
  - L389: 1. **Exception System:** Currently throws C++ exceptions, should use PSQL exception stack
  - L390: 2. **WHILE Loop:** Body parsing may need refinement (comment suggests simplification)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/SQL_IDENTIFIER_UTF8_FIX_PLAN.md
- Potential ambiguity markers detected:
  - L531: // 64 Chinese characters = 192 bytes (should fit in 512)
  - L644: -- Test 5: Should fail (exceeds character limit)
  - L1126: ### Future Enhancements (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/planning_archive (1)/TOAST_MGA_COMPLIANCE_FIX_PLAN.md
- Potential ambiguity markers detected:
  - L337: #### Task 1.6: Migration Strategy (Optional)
  - L372: - TODO comments added for Phase 2 (TIP-based visibility checks)
  - L691: - TODO comments added for future enhancement
  - L702: 1. Transaction A writes TOAST, B reads (should see)
  - L703: 2. Transaction A writes TOAST but aborts, B reads (should NOT see)
  - L723: - **Indexes should NOT detoast values themselves**
  - L843: **Complexity**: GIN indexes arrays and text - may need to detoast array elements individually.
  - L1299: - [ ] Aborted transactions cleaned up (TODO: xmax clearing)
  - L1336: - TODO: Clear xmax for chunks where delete transaction aborted (line 1407-1409)
  - L1444: - **Replication** (shipping changes to replicas) - optional feature
  - L1445: - **Point-in-time recovery** (PITR) - optional feature
  - L1446: - **Audit logging** - optional feature
  - L1707: grep -r "detoastIfNeeded" src/core/*index*.cpp | wc -l  # Should be 7+
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/BITMAP_INDEX_COMPLETION_SPEC.md
- Potential ambiguity markers detected:
  - L89: // TODO: Implement multi-page dictionary
  - L317: // 7. Insert split key into parent (may cause recursive split)
  - L355: stats.compression_ratio = 1.0; // TODO: Calculate actual compression
  - L450: 5. [ ] Verify ratio after deletes (should recalculate)
  - L467: // TODO: Handle mixed types
  - L616: 4. [ ] Query with only first column (should work)
  - L650: - [ ] Measure search time with multi-page dictionary (should be O(log n))
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/BRIN_INDEX_COMPLETION_SPEC.md
- Potential ambiguity markers detected:
  - L113: // TODO: Implement actual range removal and compaction  ← STUB!
  - L207: // TODO: In Phase 2 (multi-page), traverse all BRIN pages via sibling pointers
  - L339: 2. [ ] Vacuum with no dead ranges (should be no-op)
  - L469: // This page should contain the range
  - L948: stats_out->avg_range_selectivity = 0.0; // TODO: Calculate  ← PLACEHOLDER!
  - L1111: stats_out->total_pages = 1; // TODO: Count actual pages in multi-page implementation
  - L1165: - [ ] Insert throughput: sequential inserts (should be O(1) per insert)
  - L1170: - [ ] Space efficiency: BRIN vs B-Tree (should be 90%+ savings)
  - L1223: 1. Feature 2 (Multi-Page) should be done before Feature 3 (Revmap)
  - L1256: - [x] Current implementation uses `TransactionId` (uint64_t), not `Snapshot`
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/GIST_INDEX_COMPLETION_SPEC.md
- Potential ambiguity markers detected:
  - L110: // TODO: Implement entry lookup and deletion
  - L343: 2. [ ] Delete non-existent entry (should succeed with NOT_FOUND)
  - L384: // TODO: Distribute entries to left and right pages
  - L385: // TODO: Compute union predicates for both pages
  - L774: // TODO: Traverse tree and physically remove entries where xmax < oldest_active_xid
  - L808: - Should be interruptible (long-running operation)
  - L809: - Should handle concurrent reads (use shared_lock where possible)
  - L1006: 2. [ ] GC with no dead entries (should be no-op)
  - L1012: 8. [ ] Concurrent reads during GC (should see consistent state)
  - L1107: - [x] Current implementation uses `TransactionId` (uint64_t), not `Snapshot`
  - L1149: - `distance()` - Optional, for k-NN queries
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/HNSW_INDEX_COMPLETION_SPEC.md
- Potential ambiguity markers detected:
  - L763: stats_out->deleted_nodes = 0; // TODO: Count from page
  - L766: stats_out->avg_connections = 0.0; // TODO: Calculate
  - L767: stats_out->avg_path_length = 0.0; // TODO: Calculate
  - L774: - Cannot monitor index health (deleted_nodes unknown)
  - L775: - Cannot assess graph quality (avg_connections unknown)
  - L776: - Cannot optimize search parameters (avg_path_length unknown)
  - L886: **Step 2: Add Sample-Based Path Length (Optional Enhancement)** (2-4 hours)
  - L1014: | 3.2 Optional: Sample-based path length | 2-4 | 3.1 |
  - L1049: - [x] Current implementation uses `TransactionId` (uint64_t), not `Snapshot`
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/LOW_LEVEL_SPECIFICATION_B-TREE_INDEX.md
- Potential ambiguity markers detected:
  - L177: This is the most complex operation. The function btree\_insert will recursively descend the tree and, upon returning, may trigger a page split.
  - L232: //    call that may cause the parent page to split as well.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/LOW_LEVEL_SPECIFICATION_HASH_INDEX.md
- Potential ambiguity markers detected:
  - L105: Insertion involves hashing the key, finding the right bucket, and adding the entry. If the bucket is full, it may need to be split, which can trigger a directory expansion.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/specifications_archive/index_completion_specs_2025/SPGIST_INDEX_COMPLETION_SPEC.md
- Potential ambiguity markers detected:
  - L134: // TODO: Allocate child pages and distribute values
  - L505: // TODO: Implement entry lookup and deletion
  - L750: 2. [ ] Delete non-existent entry (should succeed with NOT_FOUND)
  - L776: // TODO: Traverse tree and physically remove entries where xmax < oldest_active_xid
  - L809: - Should be interruptible (long-running operation)
  - L810: - Should handle concurrent reads
  - L1011: 2. [ ] GC with no dead entries (should be no-op)
  - L1017: 8. [ ] Concurrent reads during GC (should see consistent state)
  - L1114: - [x] Current implementation uses `TransactionId` (uint64_t), not `Snapshot`
  - L1249: This shows what a complete quad-tree operator class pickSplit() should look like:
  - L1316: This shows what a radix tree operator class pickSplit() should look like:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-12/CURRENT_STATUS.md
- Potential ambiguity markers detected:
  - L110: - Page merging in B-tree VACUUM (TODO marker)
  - L135: - **All 15+ TODO markers resolved** - locking operational in all critical sections
  - L208: - Full Unicode case folding (TODO marker)
  - L346: **Alpha 1.2 Requirements:** See [TODO.md](../development/TODO.md) for complete type system, DOMAIN support, and advanced index types.
  - L379: **TODO/FIXME Markers:** ~20 (reduced from 42+ - Phase 2 resolved 15+ locking TODOs, Phase 4 work reduced further)
  - L527: - ✅ TODO.md updated with CRIT/HIGH/MED issue completion status
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/README.md
- Potential ambiguity markers detected:
  - L82: 3. **Confusion Reduction**: Multiple overlapping status documents, progress reports, and TODO lists were creating confusion about what was current vs. historical.
  - L99: - `/docs/development/TODO.md` - Current prioritized work items
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/audit_2025_10_06.md
- Potential ambiguity markers detected:
  - L20: - 42+ TODO/FIXME markers indicating incomplete implementations
  - L91: - ❌ Page merging in B-tree VACUUM (TODO marker)
  - L94: - ❌ Full Unicode case folding (TODO marker)
  - L127: Throughout the codebase, there are 15+ TODO comments indicating missing connection context:
  - L129: // TODO: Get proc_id from thread-local storage or connection context
  - L150: - Audit all TODO markers related to proc_id
  - L164: // TODO: Implement cross-page update
  - L200: - Potential undefined behavior if buffer is actually const
  - L231: 3. **Some use optional ErrorContext:**
  - L464: - Should use DEBUG_LOG macro instead
  - L513: // TODO: Implement full UCA and locale-specific comparison
  - L517: - Timezone comparisons may be incorrect for some locales
  - L546: - Should probably return error instead
  - L558: ### LOW-001: TODO Markers for Future Features
  - L563: Many TODO markers throughout:
  - L564: - "TODO(concurrency): Get proc_id from thread-local storage" (15+ instances)
  - L565: - "TODO: Implement page merging" (btree_vacuum.cpp:328)
  - L566: - "TODO: Full integration of collation-aware" (catalog_manager.h:148)
  - L574: - Add issue numbers to TODO comments
  - L612: // TODO: comment
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/doc_audit.md
- Potential ambiguity markers detected:
  - L187: 1. Specified as "lock-free reads" but current implementation uses std::mutex
  - L671: - Algorithms listed (LZ4, Zstd) but integration not specified
  - L672: - When to compress, when to decompress not defined
  - L683: - Lock escalation policies not defined
  - L688: - Some docs suggest WAL is optional (ARCHITECTURE_CLARIFICATION.md)
  - L758: - Migration path from OID to UUID not defined
  - L763: - Upgrade path not defined
  - L768: - Migration scripts needed but not specified
  - L840: 2. **Context-Aware Parsing:** Needs symbol resolution (partial), semantic analyzer (missing), scope management (undefined)
  - L841: 3. **JIT Compilation:** Needs profiling data (spec'd), hot path detection (undefined), native code gen (no spec)
  - L859: 1. **Y-Valve process model undefined** - Process-per-connection vs. thread-per-connection
  - L860: 2. **Page size policy undefined** - Global vs. per-table vs. per-object
  - L861: 3. **SBLR versioning policy undefined** - Backward compatibility strategy
  - L862: 4. **Catalog evolution policy undefined** - Schema migration strategy
  - L984: - Some docs: "WAL is optional"
  - L986: - **Resolution:** Clarify: "MGA provides ACI without WAL. WAL adds D (durability). Optional for in-memory mode, required for durable mode."
  - L1003: - Implementation strategy not specified
  - L1087: - Design specs: What should be built (ARCHITECTURE_GOALS.md)
  - L1256: 7. **Testing gap** - Database init hang blocks test execution, quality unknown
  - L1257: 8. **Performance unknown** - No benchmarks, no optimization, no profiling
  - ... 3 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/error_handling_audit_2025_10_07.md
- Potential ambiguity markers detected:
  - L18: **Key Finding:** The `SET_ERROR_CONTEXT` macro already includes a nullptr check (line 57 in error_context.h), making it safe to call with nullptr. However, function signatures are inconsistent about whether ErrorContext is optional or required.
  - L20: **Recommendation:** Standardize on **Option B** (Always optional with macro safety check) with clear documentation.
  - L26: ## Current Implementation Analysis
  - L55: ### Pattern 1: Optional ErrorContext (Most Common)
  - L102: - Internal functions that should always have context from caller
  - L107: // Same as optional - macro still checks
  - L131: ### Inconsistency 1: Optional vs Required Not Clear
  - L133: **Issue:** No clear guideline for when to make ErrorContext optional vs required.
  - L144: // Why is this optional?
  - L165: **Should Be:**
  - L170: * @param ctx Error context (optional, can be nullptr)
  - L192: // Should also set error context if available
  - L205: - Optional ErrorContext (`= nullptr`): ~425 calls (85%)
  - L212: - ⚠️ Inconsistent function signatures (optional vs required)
  - L218: ### Option B: ErrorContext Always Optional with Macro Safety
  - L231: - ✅ Consistent with modern C++ practice (std::optional-like semantics)
  - L235: // Public API - always optional
  - L279: ### Step 2: Add Helper Macros (Optional Enhancement)
  - L299: Add Doxygen comments clarifying that ErrorContext is optional:
  - L303: * @param ctx Error context (optional, can be nullptr)
  - ... 11 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/reconciliation_report.md
- Potential ambiguity markers detected:
  - L62: - Create honest "Current Implementation Status" document
  - L161: - **Impact:** Segmentation fault, undefined behavior, memory corruption
  - L251: - **Impact:** WHERE clauses may not work correctly
  - L345: - Latin-1 char 0x80 should map to 0xC2 0x80, but produces 0xC2 0x00
  - L366: 2. **UTF-16/32 are non-functional stubs** - should not be listed as "defined"
  - L372: - Should be marked as "INFRASTRUCTURE ONLY - NOT FUNCTIONAL"
  - L387: - Comment: "TODO(timezone): Implement full DST calculation based on date"
  - L423: | **CLOG (Commit Log)** | ✅ Complete - 160x space savings | **MISSING FROM AUDIT** | #22 | Transaction commit status may not work |
  - L566: **Why Trustworthy:** These accurately describe the VISION and DESIGN GOALS, not claiming current implementation.
  - L640: This is a DESIGN SPECIFICATION, not a description of current implementation.
  - L786: #### MEDIUM (Can Defer but Should Fix)
  - L882: - May contain marketing claims not grounded in reality
  - L886: - `ALPHA_IMPLEMENTATION_PLAN.md` - Roadmap may be unrealistic
  - L1007: - Should be marked: "Infrastructure only - not production ready"
  - L1177: - `/docs/specifications/parser/v3/status/` - Current implementation reality
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/repair.md
- Potential ambiguity markers detected:
  - L80: - **Category**: Partial Implementation / TODO
  - L82: - **Description**: Prefix compression is marked as TODO (line 71), and functions like `get_node()` have partial decompression logic (line 317) but return compressed keys without actually decompressing. `enableCompression()` (line 339), `getPagePrefix()` (line 362), and `calculateNodePrefix()` (line 374) are incomplete stubs.
  - L124: - **Description**: The `updateTuple()` method doesn't check if the old or new tuple contains TOAST pointers. When updating a TOASTed tuple, the old TOAST chunks should be deleted and new ones created, but this logic is absent.
  - L132: - **Description**: In `insertTuple()`, the code reuses deleted slots by checking `items[i].isDeleted() && items[i].length >= actual_tuple_size`. However, deleted items may have invalid offsets. The `isValid()` check should be performed before reusing.
  - L150: - **Description**: The code rejects compression if `dst->size() >= src_size * 0.9`, returning `Status::INVALID_ARGUMENT`. This is a poor status code for "compression not beneficial" - should be a specific status or just use uncompressed.
  - L158: - **Description**: In `createToastTable()`, if the index creation fails, the code only logs (commented TODO) but continues. Later operations like `readToastChunks()` and `deleteToastValue()` fall back to heap scans, which are O(N) instead of O(log N).
  - L205: - **Impact**: Segmentation fault, undefined behavior, memory corruption
  - L220: - **Description**: In `getSnapshot()`, `snapshot_out.active_xids.clear()` is called but may cause reallocation on every call. The vector should be reserved or reused.
  - L258: - **Impact**: WHERE clauses may not work correctly
  - L265: - **Description**: The comment correctly notes that `bytecode_` stores a raw pointer and the caller must ensure lifetime, but there's no enforcement. If a caller passes a temporary vector that goes out of scope, undefined behavior results.
  - L273: - **Description**: `readString()` validates maximum string length (16MB), but doesn't validate that `pc_ + length <= bytecode_size_` BEFORE the check. While line 214 does this check, the ordering should be: size check first, then MAX check, to prevent integer overflow if `length` is malicious.
  - L297: - **Description**: The executor manually builds tuple binary format with `TupleHeader` and null bitmap, but this duplicates logic that should be in a serialization layer. The null bitmap offset calculation (line 487-490) doesn't match HeapPage's expectations.
  - L316: - **Impact**: Parser may not handle all SQL types
  - L349: - **Description**: When `std::from_chars()` fails, the code calls `makeError()`, but the function definition wasn't shown. If makeError() doesn't properly populate the token, downstream code may crash.
  - L387: - **Description**: In `lessThan()` and `greaterThan()`, there's no null handling. If either operand is NULL, the comparison should return NULL (or false in three-valued logic), but the code will attempt string comparison and may crash.
  - L412: - **Description**: DECIMAL values are serialized by calling `toString()` then writing the string. This is extremely inefficient - should use binary packed decimal format.
  - L437: - **Impact**: NULLs may serialize incorrectly
  - L450: - **Description**: `convertFromLatin1ToUTF8()` converts Latin-1 characters 0x80-0xFF to 2-byte UTF-8 sequences. However, the calculation `output.push_back(0xC0 | ((ch >> 6) & 0x03))` is incorrect. Latin-1 char 0x80 should map to 0xC2 0x80, but this produces 0xC2 0x00.
  - L466: - **Description**: `loadFromCatalog()` queries `pg_charset` and `pg_collation` system catalogs, but these catalogs are never populated. The code calls `getTable()` which may fail silently.
  - L482: - **Category**: Partial Implementation / TODO
  - ... 7 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/status_update.md
- Potential ambiguity markers detected:
  - L267: - `shouldMergePages()`: Lines 1423-1467 - Determines if adjacent pages should be merged (80% threshold)
  - L298: - TODO added for multi-user context via thread-local storage
  - L538: - ✅ Validates days per month: Jan=31, Feb=28/29, Mar=31, Apr=30, May=31, Jun=30, Jul=31, Aug=31, Sep=30, Oct=31, Nov=30, Dec=31
  - L648: - Allows decimal point with optional following digits
  - L677: - Previous: If index creation failed, logged TODO and returned OK
  - L684: // This is not fatal, but we should log it
  - L685: // TODO: Add logging
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/analysis/DATATYPE_AUDIT_REPORT.md
- Potential ambiguity markers detected:
  - L45: UNKNOWN = 0,
  - L109: // Result: Type mismatch, undefined behavior
  - L310: **Problem:** semantic_analyzer.cpp:182-224 uses parser::DataType but should use core::DataType.
  - L666: UNKNOWN = 0,
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/CATALOG_SYSTEM_AUDIT_2025_10_03.md
- Potential ambiguity markers detected:
  - L14: The ScratchBird catalog system manages database metadata through a fixed-page structure with in-memory caching. This audit identifies critical issues with the current implementation, particularly regarding new data types and missing metadata fields.
  - L173: - `relnatts` (should match column_count but useful for validation)
  - L283: - Should use TOAST for large defaults
  - L428: - Should use dynamic array or TOAST
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/COMPILATION_AUDIT_REPORT.md
- Potential ambiguity markers detected:
  - L138: **Root Cause:** Destructor was declared in header but not defined, causing linker error
  - L192: - Function `findRecordInHeapPage<T>()` is called but not defined
  - L205: // ^^^ UNDEFINED FUNCTION
  - L209: **TODO Comment Found:**
  - L212: // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
  - L246: - Function `scanHeapPage<RecordType, InfoType>()` not defined
  - L248: - Should scan all records in heap page and convert to InfoType
  - L250: **TODO Comment Found:**
  - L253: // TODO: Needs scanHeapPage helper function
  - L393: **Priority:** P4 - Optional, personal preference
  - L447: // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
  - L805: ### 4.6 LOW: Rename Short Identifiers (Optional)
  - L894: ### 7.2 Quality Improvements (Optional)
  - L919: 3. **Test with existing code** - Lines 1623, 1658, 1711, 1721 should compile
  - L1047: - ⚠️ Address ~1,388 style warnings (optional, P3-P4 priority)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/btree_fix_report.md
- Potential ambiguity markers detected:
  - L222: 1. All existing B-tree tests should pass
  - L223: 2. Hash index tests should be unaffected
  - L224: 3. Transaction manager tests should pass
  - L340: The critical B-Tree navigation bug has been **FIXED**. All index operations should now work correctly. The fix adds a rightmost child pointer to the page header and updates all split/navigation logic to maintain it properly.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/bufferpool_pin_unpin_analysis_report.md
- Potential ambiguity markers detected:
  - L105: - This is exactly what should happen
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/compilation_fixes_summary.md
- Potential ambiguity markers detected:
  - L271: **Priority:** P3-P4 (optional style improvements)
  - L305: **Note:** Current implementation is sufficient for typical workloads.
  - L330: **Next Steps:** Run test suite, address optional style warnings as time permits
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/cross_page_version_chains_fix_report.md
- Potential ambiguity markers detected:
  - L27: **Key Innovation:** Snapshots are **required** (not optional), ensuring all callers benefit from safe cross-page pointer returns without special case handling. Pages are pinned for the entire snapshot lifetime and cleaned up automatically when the transaction commits or rolls back.
  - L69: (started before UPDATE)         Should see Tuple A (xmin=100)
  - L321: - **VISIBLE!** This is the version the snapshot should see
  - L326: **Solution:** Return `NOT_IMPLEMENTED`, caller should use `getTupleDetoasted()` which copies data
  - L403: **Key Change:** Added optional `snapshot` parameter (defaults to nullptr for backward compatibility)
  - L608: 7. Should be: xmin=100, xmax=0 originally
  - L719: - Added optional `snapshot` parameter (defaults to nullptr)
  - L852: - ✅ Backward compatible (snapshot parameter optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/executor_tuple_fix_report.md
- Potential ambiguity markers detected:
  - L57: **Actual Data Flow (What Should Happen):**
  - L170: | `infomask` | 2 bytes | ✅ YES | 🔄 May modify | Tuple state flags (has nulls, etc.) |
  - L223: ✅ No undefined behavior from garbage padding
  - L270: 2. **Metadata Preservation:** Some fields like `infomask` and `null_bitmap_offset` are known at serialization time and should be set by the executor
  - L274: 4. **Update Operations:** When updating, the old tuple's header fields may need to be copied/preserved
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/page_lock_management_fix_report.md
- Potential ambiguity markers detected:
  - L490: **Exception:** Single atomic reads (e.g., reading one field from page header) may not need locks if hardware guarantees atomicity.
  - L503: - Unlock unlocked page → undefined (implementation detail)
  - L513: - With timeout: Should timeout instead of deadlock
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/pointer_arithmetic_bounds_checking_fix_report.md
- Potential ambiguity markers detected:
  - L81: 4. **Defensive validation** even when operations "should" be safe (e.g., after insertTuple)
  - L134: // Validate new item pointer bounds (should always be valid after insertTuple, but check defensively)
  - L291: The following scenarios should be tested:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/pointer_safety_elimination_report.md
- Potential ambiguity markers detected:
  - L9: **Solution:** Made snapshot parameter required instead of optional
  - L18: **This limitation has been completely eliminated** by making the snapshot parameter **required** instead of optional. Now:
  - L27: ## Problem: Optional Snapshot Created Two Code Paths
  - L29: ### Original Design (Option 3 with Optional Snapshot)
  - L34: TransactionManager::Snapshot *snapshot = nullptr,  // OPTIONAL
  - L205: **Before (Optional Snapshot):**
  - L278: **Before (Optional Snapshot):**
  - L420: ### Before (Optional Snapshot)
  - L432: **Key Insight:** Making snapshots **required** instead of **optional** transforms Option 3 from "mostly works with fallback" to "always works perfectly".
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/timestamp_timezone_fix_report.md
- Potential ambiguity markers detected:
  - L326: **New format:** 9-11 bytes (flags + optional timezone + timestamp)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/tip_chaining_fix_report.md
- Potential ambiguity markers detected:
  - L37: // In production, we'd handle page overflow and chaining  // <-- TODO acknowledged
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/toast_integration_fix_report.md
- Potential ambiguity markers detected:
  - L28: - Low-level component, should NOT know about storage details
  - L146: - CatalogManager should provide ToastManager per table
  - L246: - CREATE TABLE should auto-create TOAST table
  - L247: - Catalog should track TOAST relationships
  - L288: **None.** However, existing databases may have tuples that should have been TOASTed but weren't. Consider:
  - L315: // Insert 1KB tuple → should NOT toast
  - L316: // Insert 10KB tuple → should TOAST
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/toast_integration_implementation_report.md
- Potential ambiguity markers detected:
  - L286: 2. **Optional: Create TOAST tables**
  - L292: 3. **Optional: Retroactive TOASTing**
  - L303: Currently, ToastManager initialization fails if TOAST table doesn't exist. We should:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/toast_thread_safety_fix_report.md
- Potential ambiguity markers detected:
  - L325: - First 5 should succeed
  - L326: - 6th should return `Status::RESOURCE_EXHAUSTED`
  - L400: - Older ARM: May use LL/SC (load-linked/store-conditional) - slight overhead
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/type_conversion_fix_report.md
- Potential ambiguity markers detected:
  - L114: throw std::runtime_error("Unknown data type opcode");
  - L180: throw std::runtime_error("Unknown data type opcode");
  - L207: default: throw std::runtime_error("Unknown type");
  - L408: - Old executor would throw "Unknown data type opcode" for new types
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/type_serialization_verification_report.md
- Potential ambiguity markers detected:
  - L153: 1. **Audit based on assumptions**: The auditor may have assumed DECIMAL used a different API without reading the actual code
  - L154: 2. **Confused with documentation**: May have read design docs that described a different implementation
  - L155: 3. **Copy-paste error in audit**: The issue description may have been incorrectly copied from another source
  - L156: 4. **Different branch**: Audit may have been done on a different branch (unlikely, as only one commit exists)
  - L223: This issue should be **removed from the critical issues list** and **not block any work**.
  - L233: These are **optimization opportunities**, not bugs. Current implementation:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/update_tuple_toast_fix_report.md
- Potential ambiguity markers detected:
  - L19: > "The `updateTuple()` method doesn't check if the old or new tuple contains TOAST pointers. When updating a TOASTed tuple, the old TOAST chunks should be deleted and new ones created, but this logic is absent."
  - L95: // Insert new version (may create new TOAST chunks)
  - L107: 1. Old tuple may be TOASTed (contains ToastPointer, actual data in TOAST table)
  - L109: 3. New tuple is inserted (may create NEW TOAST chunks via `insertTuple()`)
  - L224: - TOAST chunks may have been cleaned up by VACUUM
  - L225: - Concurrent transaction may have deleted them
  - L510: - Databases with previous updates may have orphaned TOAST chunks
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/varchar_max_length_fix_report.md
- Potential ambiguity markers detected:
  - L19: 1. Adding optional TypeInfo storage to TypedValue
  - L96: std::optional<TypeInfo> type_info_; // NEW: Optional type metadata
  - L100: auto getTypeInfo() const -> const std::optional<TypeInfo>& { return type_info_; }
  - L106: **Why optional?**
  - L109: - **Memory**: std::optional adds minimal overhead (1 byte flag + TypeInfo only if present)
  - L454: ✅ **Performance impact negligible** - 1 extra byte read + optional 4-byte read
  - L477: - **Issue #43** (MEDIUM): DECIMAL serialization (should also preserve precision/scale)
  - L488: - **Line 199**: Added `std::optional<TypeInfo> type_info_` member
  - L491: - `getTypeInfo()` - Returns const reference to optional TypeInfo
  - L497: - TypedValue size increases by sizeof(std::optional<TypeInfo>)
  - L500: - Modern compilers optimize std::optional to minimal overhead
  - L512: - Optional precision reading based on flags
  - L567: - Should fail validation (if implemented)
  - L583: // Should store precision AND scale
  - L592: // Should store timezone
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/xid_validation_analysis.md
- Potential ambiguity markers detected:
  - L24: ## Analysis of Current Implementation
  - L305: 2. Tuple is **ALWAYS VISIBLE** (may or may not be correct)
  - L331: ## What Should Happen
  - L373: // TODO: Check if XID is too old (vacuumed away)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/repair_work/xid_validation_enhancements_report.md
- Potential ambiguity markers detected:
  - L161: - Ancient transaction that should have been frozen (should be FROZEN_XID)
  - L249: // XIDs older than oldest_xid_ should have been frozen by VACUUM
  - L252: // Old XID that should have been frozen
  - L255: fprintf(stderr, "[WARNING] XID %lu is older than oldest_xid %lu - tuple should have been frozen by VACUUM\n",
  - L258: // In strict mode, this should return false
  - L386: // Corrupted - oldest_xid should never exceed next_xid
  - L438: fprintf(stderr, "[WARNING] XID %lu is older than oldest_xid %lu - tuple should have been frozen by VACUUM\n",
  - L537: [WARNING] XID <xid> is older than oldest_xid <oldest> - tuple should have been frozen by VACUUM
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/development/TODO.md
- Potential ambiguity markers detected:
  - L1: # Todo
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/DEFICIENCY_ANALYSIS_AND_ACTION_PLAN.md
- Potential ambiguity markers detected:
  - L125: 1. **Parser Rewrite** - May require significant refactoring
  - L127: 3. **Performance** - Current architecture may have limitations
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/DEFICIENCY_CORRECTION_PLAN.md
- Potential ambiguity markers detected:
  - L181: 2. **Parser Rewrite**: May require significant refactoring
  - L182: 3. **Performance**: Current architecture may have bottlenecks
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/IMPLEMENTATION_PROGRESS_REPORT.md
- Potential ambiguity markers detected:
  - L40: - ✅ Compression support (LZ4) - Optional, functional
  - L76: - 15+ TODO markers across codebase
  - L92: // TODO(concurrency): Get proc_id from thread-local storage
  - L119: - Has TODO marker
  - L213: - ❌ Full Unicode case folding (TODO marker)
  - L376: - **TODO Markers:** 42+
  - L387: - ✅ [TODO.md](../development/TODO.md) - Updated with audit findings
  - L538: - [Current Status](../status/CURRENT_STATUS.md) - Current implementation status
  - L539: - [TODO.md](../development/TODO.md) - Prioritized work items
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/ISSUE-001-test-failures.md
- Potential ambiguity markers detected:
  - L15: During the Alpha 1.03 System Catalog implementation, several pre-existing test failures were identified. These tests fail due to outdated assumptions, API changes, or testing complex edge cases that may not be relevant for the Alpha phase.
  - L31: - `PageManagementTest.BufferPoolDirtyPages` - Intermittent failures, may indicate real issue
  - L34: **Status**: Most fixed, some may still have issues
  - L57: 1. Review and fix `PageManagementTest.BufferPoolDirtyPages` - may be a real bug
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/OUTDATED_REPORTS_UPDATE.md
- Potential ambiguity markers detected:
  - L67: ## Reports That Should Be Updated
  - L69: 1. **tests/TEST_EXECUTION_REPORT.md** - Should note that memory safety and ErrorContext issues are fixed
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/issues/TIP_CORRUPTION_FIX_REPORT.md
- Potential ambiguity markers detected:
  - L121: 3. **FSM is Authoritative**: The Free Space Map should be the authoritative source for page allocation information, not the database header
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/CRITICAL_FIXES_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L64: **Current Implementation (btree.cpp):**
  - L184: You should define **all of them** (or explicitly delete them).
  - L374: // or should be deleted here if allocated with new
  - L541: // Should be roughly double
  - L693: EXPECT_EQ(s, Status::PAGE_CORRUPT);  // Should reject, not crash
  - L729: - If multiple .cpp files include this header and use this template, linker may complain
  - L730: - Should be marked `inline` to prevent ODR violations
  - L805: ### Phase 2: High-Priority Fixes (Should Do Next)
  - L833: // ErrorContext ctx2 = ctx1;  // Should not compile
  - L931: 1. Each fix should be a separate commit
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/MGA_GAP_ANALYSIS.md
- Potential ambiguity markers detected:
  - L40: ### Current Implementation:
  - L137: ### Current Implementation:
  - L215: ### Current Implementation:
  - L305: ### Current Implementation:
  - L377: ### Current Implementation:
  - L435: ### Current Implementation:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/MGA_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L1196: // UPDATE should link old → new
  - L1200: // Snapshot should see correct version in chain
  - L1205: // Vacuum should reclaim space
  - L1209: // Vacuum should not remove visible tuples
  - L1227: // Readers should not block writers
  - L1231: // Long snapshot should block vacuum
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_1_COMPLETE.md
- Potential ambiguity markers detected:
  - L295: - Should add explicit initialization at startup
  - L344: - May be initialization order issue
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_1_INTEGRATION_GUIDE.md
- Potential ambiguity markers detected:
  - L62: **Expected Result**: All files should compile without errors.
  - L482: LOG_DEBUG(GENERAL, "This should not appear");
  - L483: LOG_INFO(GENERAL, "This should not appear");
  - L484: LOG_WARNING(GENERAL, "This should appear");
  - L485: LOG_ERROR(GENERAL, "This should appear");
  - L602: // Initialize logger (should read from config)
  - L606: // Log should go to file with DEBUG level
  - L656: # Edit configuration (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_1_INTEGRATION_STATUS.md
- Potential ambiguity markers detected:
  - L301: 2. **Logger Performance**: No performance testing yet. Logging overhead should be measured under load.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_2_PROGRESS.md
- Potential ambiguity markers detected:
  - L113: ### Task 2.4: Update All TODO Markers ✅ (5 days)
  - L127: // TODO(concurrency): Get proc_id from thread-local storage or connection context
  - L218: // TODO: Get table_id from ConnectionContext (future enhancement)
  - L407: - Update TODO.md with remaining work
  - L467: - [x] All TODO markers updated
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_3_PROGRESS.md
- Potential ambiguity markers detected:
  - L196: - **READ_COMMITTED_READ_CONSISTENCY**: Falls back to READ_COMMITTED (TODO: statement snapshots)
  - L498: b. Optional: Reclaim space (foreground only)
  - L507: - Space reclamation is optional and only in foreground sweeps
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_1_PHYSICAL_TUPLE_REMOVAL_COMPLETE.md
- Potential ambiguity markers detected:
  - L108: - **DELETED**: flags = 1 (logically deleted, may have version chain)
  - L306: - Safe for compaction where tuples may overlap during move
  - L320: - DELETED tuples may have version chains (not pruned)
  - L376: Future testing should include:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_2_CONDITION_VARIABLE_COMPLETE.md
- Potential ambiguity markers detected:
  - L38: T=0s:   wakeBackgroundThread() does nothing (TODO)
  - L136: Replaced TODO with actual implementation:
  - L142: // TODO: Implement proper wake mechanism (condition variable)
  - L212: - Thread may be waiting on condition variable
  - L295: Future testing should include:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_3_ENHANCED_METRICS_COMPLETE.md
- Potential ambiguity markers detected:
  - L230: - Validate tuning changes (distribution should improve)
  - L498: Future testing should include:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_4_ADAPTIVE_TUNING_COMPLETE.md
- Potential ambiguity markers detected:
  - L610: Future testing should include:
  - L722: **Current**: Best-effort GC (may impact queries)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_5_PRIORITY_QUEUE_COMPLETE.md
- Potential ambiguity markers detected:
  - L388: - May clean pages with little garbage
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/planning/PHASE_4_PART_6_COMPREHENSIVE_TESTS_COMPLETE.md
- Potential ambiguity markers detected:
  - L131: // GC should be enabled by default
  - L134: // Default policy should be COMBINED
  - L257: // Mark same page again (should increase mark_count but not dirty count)
  - L325: // All counters should start at 0
  - L416: // Try to start again - should fail
  - L423: // Try to stop again - should fail
  - L468: // Check statistics - should have at least one background run
  - L535: // Current tuning parameters should be set to defaults
  - L577: // Mark page 300 once (should be newest)
  - L582: // Should have tracked all marks
  - L659: // Should have tracked all 300 marks
  - L706: // Should have run but found no tuples
  - L747: // Should have run but cleaned no pages
  - L840: // Should have marked 1000 pages
  - L880: // Should complete in reasonable time (< 1000ms)
  - L919: // Should complete in reasonable time (< 100ms)
  - L1060: **Lesson**: GarbageCollector tests should focus on GC behavior, not internal HeapPage details
  - L1106: **Lesson**: Accumulation metrics should count total events, not just unique items
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/BTREE_PHASE3_RANGE_SCAN_COMPLETE.md
- Potential ambiguity markers detected:
  - L335: **Current Implementation:**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/BTREE_PHASE4_COMPRESSION_COMPLETE.md
- Potential ambiguity markers detected:
  - L191: new_node->btn_prefix_len = 0; // TODO: Implement prefix compression
  - L312: - ⚠️ Not yet integrated into add_node() (line 71 still TODO)
  - L314: - ⚠️ Decompression returns compressed key (TODO at line 317)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/BTREE_PHASE5_VACUUM_COMPLETE.md
- Potential ambiguity markers detected:
  - L144: **Implementation Status:** ⚠️ Decision logic complete, merge operation is TODO for production
  - L211: - May slow down writes slightly
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/HASH_INDEX_STATUS.md
- Potential ambiguity markers detected:
  - L214: 4. **Compression** - Add optional key compression
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/LEGACY_PROJECT_STATUS.md
- Potential ambiguity markers detected:
  - L170: - TODO comment resolution
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_IMPLEMENTATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L426: ### Current Implementation:
  - L476: ### Phase 5: CLOG Optimization (Optional)
  - L550: - ⏳ CLOG optimization (Phase 5) - Optional
  - L571: 1. **Connection Context First:** Should have designed before APIs
  - L603: 3. Cross-page version chains (optional)
  - L604: 4. Auto-vacuum (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_PHASE1_COMPLETE.md
- Potential ambiguity markers detected:
  - L192: - Added TODO comments for future proc_id threading
  - L374: **Impact:** Unknown edge cases may exist
  - L380: **Impact:** May not work on all platforms
  - L410: **Status:** Some call sites may still need updates (tests, etc.)
  - L428: 1. **Connection context** - Should have designed this first
  - L429: 2. **Testing** - Should have TDD approach
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_PHASE2_COMPLETE.md
- Potential ambiguity markers detected:
  - L205: - Added TODO comments for future lock acquisition
  - L477: 1. **Testing** - Should have written tests alongside code
  - L478: 2. **Connection Context** - Should have designed this earlier
  - L479: 3. **Lock Timeout** - Stub implementation should be functional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/MGA_PHASES_3_4_COMPLETE.md
- Potential ambiguity markers detected:
  - L631: - Phase 5: CLOG Optimization (optional - 160x space savings)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/status/OVERALL_PROJECT_STATUS.md
- Potential ambiguity markers detected:
  - L180: ### Should Have 🟡
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/DOCUMENTATION_REORGANIZATION_2025_10_23.md
- Potential ambiguity markers detected:
  - L193: - Compression (12-16 hours) - SHOULD HAVE
  - L194: - Encryption (14-18 hours) - SHOULD HAVE
  - L383: ## Missing Documentation (Still To Create - Optional)
  - L416: ### Future (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/SESSION_PROGRESS_2025_10_28_CONDITIONAL_FUNCTIONS.md
- Potential ambiguity markers detected:
  - L36: - Optional else_result
  - L66: - [ ] Handle optional ELSE clause
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/SESSION_SUMMARY_2025_10_23_MGA_CATALOG_COMPLIANCE.md
- Potential ambiguity markers detected:
  - L16: This session completed critical MGA compliance fixes for the ScratchBird catalog system and implemented catalog garbage collection. Two architectural bugs were identified and fixed, comprehensive unit tests were implemented, and an optional garbage collection optimization was added.
  - L99: - **Catalog Garbage Collection**: OPTIONAL optimization (2-3 hours) - IMPLEMENTED
  - L596: ### 3. Garbage Collection is Optional But Valuable
  - L611: This session successfully addressed two critical MGA compliance violations in the ScratchBird catalog system and implemented an optional garbage collection optimization. All objectives were completed:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/SESSION_SUMMARY_2025_10_24_CONTEXT_VARIABLES_DESIGN.md
- Potential ambiguity markers detected:
  - L38: - Optional precision for time types (e.g., `CURRENT_TIME(3)`)
  - L108: mutable std::optional<Date> current_date_cache_;
  - L109: mutable std::optional<TimeWithTZ> current_time_cache_;
  - L110: mutable std::optional<TimestampWithTZ> current_timestamp_cache_;
  - L262: - Simple implementation (depth counter + optional cache)
  - L417: - Create skeleton tests with TODO comments
  - L437: 1. **Consider Optional Row UUID**: Make `rdb$row_uuid` optional per-table to avoid +16 byte overhead
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-10-sessions/SESSION_SUMMARY_2025_10_28_JSON_FUNCTIONS_PRODUCTION.md
- Potential ambiguity markers detected:
  - L181: - Production TODO: Replace stubs, JSONPath, Integration tests
  - L189: - Performance optimization: ⏸️ DEFERRED (optional)
  - L215: - ⏸️ JSONB binary format optimization deferred (optional)
  - L248: **Remaining Work (Optional)**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALL_INDEX_WORK_COMPLETE_2025-11-06.md
- Potential ambiguity markers detected:
  - L267: 2. ✅ All TODO comments addressed
  - L368: ### Optional Next Steps
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALPHA_003_AUDIT_FINDINGS.md
- Potential ambiguity markers detected:
  - L103: - Status: Framework ready, algorithms TBD
  - L109: - Status: Framework ready, optimization TBD
  - L194: - Status: Works correctly, optimization TBD
  - L198: - Comment: "TODO: Count overflow pages"
  - L199: - Status: Basic stats work, overflow count TBD
  - L254: 2. **Update TODO.md** (Priority: HIGH)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALPHA_003_PROGRESS.md
- Potential ambiguity markers detected:
  - L35: | **GIST** | ❌ NOT STARTED | TBD | 0 | 0% | MEDIUM | Extensibility framework |
  - L36: | **BRIN** | ❌ NOT STARTED | TBD | 0 | 0% | MEDIUM | For very large tables |
  - L37: | **VECTOR** | ❌ NOT STARTED | TBD | 0 | 0% | MEDIUM | For similarity search |
  - L104: - Compression algorithms (structures defined, full implementation TBD)
  - L105: - UUIDv7 range pruning optimization (fields present, optimization TBD)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/ALPHA_ENGINE_READINESS_SUMMARY.md
- Potential ambiguity markers detected:
  - L38: **Current Implementation**: ~78% of specified features (+18% from DDL completion)
  - L330: - TODO: "Implement proper LIKE with % and _ wildcards"
  - L413: - **Note**: `bytecode_generator.cpp:3165` - "TODO: Implement ELSIF generation"
  - L483: - TODO: "Implement CHECK constraint evaluation"
  - L770: - ABS, SIGN (may exist, verify)
  - L772: - MOD (may exist as % operator)
  - L774: - POWER (may exist as ^ operator)
  - L874: ### Phase 9: Advanced Features (OPTIONAL) - 80-120 hours
  - L909: | 1. Security System | CRITICAL | 60-80 | 1.5-2 | ✅ YES | ❌ TODO |
  - L910: | 2. Views & Sequences | HIGH | 60-80 | 1.5-2 | ⚠️ Expected | ❌ TODO |
  - L911: | 3. Constraints | HIGH | 60-80 | 1.5-2 | ⚠️ Expected | ❌ TODO |
  - L912: | 4. Math Functions | HIGH | 20-30 | 0.5-1 | ⚠️ Expected | ❌ TODO |
  - L913: | 5. Index Completion | CONDITIONAL | 140-200 | 3.5-5 | ❓ Depends | ❌ TODO |
  - L914: | 6. Stored Procedures | HIGH | 80-120 | 2-3 | ⚠️ Expected | ❌ TODO |
  - L915: | 7. Trigger Execution | MEDIUM | 40-60 | 1-1.5 | ❌ NO | ❌ TODO |
  - L916: | 8. Advanced Features | LOW | 80-120 | 2-3 | ❌ NO | ❌ TODO |
  - L1072: - Correctly identified 105 TODO/FIXME markers
  - L1186: - [ ] GIN (optional for ALPHA if no full-text requirement)
  - L1187: - [ ] HNSW (optional for ALPHA if no vector search requirement)
  - L1203: - [ ] MERGE (optional, nice-to-have)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BITMAP_INDEX_COMPLETION_REPORT_2025-11-04.md
- Potential ambiguity markers detected:
  - L79: - Returns empty set if value not in index (caller should use heap scan)
  - L106: ### 3. Multi-Page Dictionary Support (Previously TODO)
  - L128: ### 4. Compression Ratio Calculation (Previously TODO)
  - L154: ### 5. Mixed Container Type Handling (Previously TODO)
  - L157: **Previous Code**: TODO comment at line 1340
  - L291: 2. **findNot()** returns empty if value not in index (caller should use heap scan)
  - L296: 1. **Root page allocation**: bitwiseAnd/bitwiseOr still have "TODO: Allocate root page" (line 1229)
  - L310: ✅ **No TODOs**: All TODO comments resolved
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BITMAP_INDEX_IMPLEMENTATION_SUMMARY.md
- Potential ambiguity markers detected:
  - L15: Successfully completed the Bitmap Index implementation by adding all missing API methods and resolving all TODO comments. The index is now fully functional, MGA-compliant, and production-ready.
  - L61: - **Before**: TODO comment at line 1102
  - L66: - Still has "TODO: Allocate root page" in bitwiseAnd/bitwiseOr
  - L177: 2. **findNot()** returns empty if value not in index (caller should use heap scan)
  - L226: **Next Steps** (Optional):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BRIN_COMPLETION_REPORT_2025-11-03.md
- Potential ambiguity markers detected:
  - L184: (TODO in Phase 1: actual compaction not implemented)
  - L513: 3. **No Summarization**: After updates, range min/max may become stale (too wide).
  - L514: - **Impact**: Scan may return more blocks than necessary
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BRIN_COMPLETION_REPORT_2025-11-04.md
- Potential ambiguity markers detected:
  - L557: ## Remaining Work (Optional Enhancements)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/BUILD_FIXES_2025-11-07.md
- Potential ambiguity markers detected:
  - L221: ### Immediate (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CATALOG_CORRECTIONS_COMPLETE_2025-11-09.md
- Potential ambiguity markers detected:
  - L335: # Should see: "System catalog initialized with 18 schemas in hierarchy"
  - L336: # Should see: "Allocated and initialized 14 new system tables (Phase 6.1)"
  - L349: -- Dependencies should be reloaded
  - L359: -- 2. Retrieve comment (should succeed)
  - L363: -- 4. Comment metadata should exist (text may be lost without TOAST)
  - L372: -- sys.parent_schema_id should = root.schema_id
  - L373: -- sec.parent_schema_id should = sys.schema_id
  - L402: - TODO: Write text to TOAST, store OID in comment_text_oid
  - L407: - TODO: Implement createUser, createRole, etc.
  - L412: - TODO: Implement createProcedure, createDomain, etc.
  - L417: - TODO: Implement CREATE EMULATION commands
  - L422: - TODO: Integrate with DROP TABLE/VIEW/etc.
  - L427: - TODO: Atomic cache+disk updates
  - L458: - Production may need lock-free structures
  - L466: **NOT SUPPORTED** in current implementation:
  - L577: 1. **Testing Earlier** - Should have added unit tests during development
  - L578: 2. **Transaction Integration** - Should have integrated with TransactionManager
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CATALOG_CORRECTIONS_PHASE1-5_COMPLETE_2025-11-09.md
- Potential ambiguity markers detected:
  - L184: ## What's In-Memory Only (Phase 6 TODO)
  - L470: 5. Squash or merge (TBD based on commit quality)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CATALOG_CORRECTION_SESSION_2025-11-08.md
- Potential ambiguity markers detected:
  - L94: Given the scope of changes, we should expect:
  - L126: Before continuing with Phase 1.1 code updates, we should decide:
  - L129: **Should we create a feature branch for this work?**
  - L145: - **Stub**: Faster initial progress, but may hide issues
  - L206: 1. Should we create a feature branch or continue on main?
  - L209: 4. Should we maintain any backward compatibility with old catalog format?
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CHECK_PARSER_COMPLETE_2025-11-13.md
- Potential ambiguity markers detected:
  - L28: - Extended constructor with optional parameters
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_COMPLETION_REPORT_2025-11-03.md
- Potential ambiguity markers detected:
  - L680: ✅ **Optional Parameters**: ErrorContext* optional throughout
  - L699: ✅ Phase 2 TODO comments
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_IMPLEMENTATION_ROADMAP_2025-11-04.md
- Potential ambiguity markers detected:
  - L77: // TODO: Implement Run-Length Encoding
  - L146: // TODO: Implement RLE decompression
  - L246: **Challenge**: Dictionary may exceed 8KB page size
  - L313: // TODO: Track min/max values
  - L314: // TODO: Compress segment
  - L315: // TODO: Write to page
  - L403: // TODO: Traverse segment chain
  - L404: // TODO: Check if tid is in range [cs_first_tid, cs_last_tid]
  - L556: - Scan with xid = 150 → should see value
  - L557: - Scan with xid = 250 → should NOT see value
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_PHASE2_DICT_COMPLETION_2025-11-04.md
- Potential ambiguity markers detected:
  - L361: Test 3: High-cardinality rejection (should fail)...
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/COLUMNSTORE_PHASES_1-4_SUMMARY_2025-11-04.md
- Potential ambiguity markers detected:
  - L210: **Why Important**: Current implementation processes values one-by-one. SIMD can process 4-8 values in parallel.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CONNECTION_CONTEXT_SECURITY_INTEGRATION_2025-11-10.md
- Potential ambiguity markers detected:
  - L134: - ⚠️ TODO: Check active_role permissions (currently only checks user's direct permissions)
  - L135: - ⚠️ TODO: Check PUBLIC permissions
  - L136: - ⚠️ TODO: Check group permissions
  - L168: - Documented TODO for full implementation
  - L177: 2. `effective_user_id_` - The user currently active (may differ after SET SESSION AUTHORIZATION)
  - L180: This is documented as a TODO for future work.
  - L246: - Log all permission checks (optional, configurable)
  - L308: #### 4. SET ROLE (Optional)
  - L332: - Test `checkPermission()` with no connection context (should deny)
  - L333: - Test `checkPermission()` for superuser (should allow all)
  - L336: - Test `checkPermission()` with zero UUID object (should deny)
  - L339: - Test SET ROLE with granted role (should succeed)
  - L340: - Test SET ROLE with non-granted role (should fail)
  - L341: - Test SET ROLE with non-existent role (should fail)
  - L342: - Test RESET ROLE (should clear active role)
  - L343: - Test SET ROLE without connection context (should fail)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CONSTRAINT_ENFORCEMENT_COMPLETE_2025-11-12.md
- Potential ambiguity markers detected:
  - L244: 2. ✅ Apply DEFAULT values to unspecified columns
  - L396: INSERT INTO test_unique VALUES (2, 'alice');  -- Should ERROR
  - L400: INSERT INTO test_unique VALUES (4, NULL);  -- Should succeed
  - L403: UPDATE test_unique SET username = 'alice' WHERE id = 3;  -- Should ERROR
  - L404: UPDATE test_unique SET username = 'bob' WHERE id = 3;    -- Should succeed
  - L417: INSERT INTO test_check VALUES (2, -5, 100.00);   -- Should ERROR
  - L418: INSERT INTO test_check VALUES (3, 25, -10.00);   -- Should ERROR
  - L429: INSERT INTO child VALUES (2, 999);   -- Should ERROR
  - L432: INSERT INTO child VALUES (3, NULL);  -- Should succeed (MATCH SIMPLE)
  - L435: DELETE FROM parent WHERE id = 1;     -- Should cascade to child
  - L436: SELECT * FROM child;                 -- Should show only (3, NULL)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/CONSTRAINT_ENFORCEMENT_PHASE1_COMPLETE_2025-11-12.md
- Potential ambiguity markers detected:
  - L26: **Functionality**: Automatically applies DEFAULT values to columns not specified in INSERT statements.
  - L192: 2. ✅ Apply DEFAULT values to unspecified columns (NEW)
  - L319: **Timeline**: Should be enabled within next 10-15 hours of work
  - L348: SELECT * FROM t1;  -- Should return (1, 'active')
  - L358: SELECT * FROM t2;  -- Should return (100, 3.14, 'hello', TRUE)
  - L366: INSERT INTO t3 VALUES (2, 'alice');  -- Should fail with UNIQUE violation
  - L370: INSERT INTO t3 VALUES (4, NULL);  -- Should succeed (multiple NULLs allowed)
  - L373: UPDATE t3 SET username = 'alice' WHERE id = 3;  -- Should fail (alice exists)
  - L374: UPDATE t3 SET username = 'bob' WHERE id = 3;    -- Should succeed
  - L382: INSERT INTO t4 VALUES (2, -5);   -- Should fail
  - L383: INSERT INTO t4 VALUES (3, 150);  -- Should fail
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DATA_TYPE_IMPLEMENTATION_STATUS.md
- Potential ambiguity markers detected:
  - L79: **Format:** Flags byte (empty, bounded, inclusive) + optional lower + optional upper
  - L107: | VARIANT | ✅ DONE | Type tag + optional serialized value |
  - L156: - 2 types may remain from the original 34 count, but they are not identified in this analysis
  - L157: - These may have been counted incorrectly or already supported
  - L158: - Further audit of the original report may be needed to identify if any truly remain
  - L300: ### Optional (Enhanced Features)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DDL_COMPLETION_REPORT_2025-11-07.md
- Potential ambiguity markers detected:
  - L325: The current implementation includes these limitations by design:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DDL_IMPLEMENTATION_STATUS.md
- Potential ambiguity markers detected:
  - L213: 4. **Enhanced dependency tracking** (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DOCUMENTATION_CLEANUP_REPORT_2025-11-03.md
- Potential ambiguity markers detected:
  - L141: **Impact:** Specification now accurately reflects current implementation (Bitmap and R-Tree were complete but undocumented).
  - L156: - **TODO Count:** Only 5 TODO/STUB markers in 3,946 lines
  - L174: - README may have been incorrectly updated to "Partial" in previous session
  - L177: **Recommendation:** User should clarify:
  - L180: - Should GIN move from "Partial" to "Complete" category?
  - L355: - Determine if these should be consolidated into MGA_IMPLEMENTATION.md
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/DOCUMENTATION_UPDATE_SUMMARY.md
- Potential ambiguity markers detected:
  - L37: - 42+ TODO/FIXME markers
  - L48: **Purpose:** Current implementation status (replaces outdated status docs)
  - L96: ### 4. `/docs/development/TODO.md` ✅ COMPLETELY REWRITTEN
  - L152: - **Standard:** What should be done
  - L155: - **Recommendations:** Links to TODO items
  - L203: **After:** Accurate snapshot of current implementation
  - L209: The following specification and design documents were **not updated** as they describe intended features rather than current implementation. They should be updated with "Implementation Status" sections in future work:
  - L212: - Should add "Status: Not Implemented" markers
  - L213: - Should cross-reference with audit report
  - L214: - Should show percentage complete
  - L215: - **Tracked in:** TODO.md DOC-001
  - L218: - May not reflect actual architecture
  - L219: - Should be verified against implementation
  - L220: - May need revision based on ConnectionContext design
  - L239: - ConnectionContext missing (15+ TODO markers)
  - L288: - Unknown time to Beta
  - L321: 2. Update TODO.md as items completed
  - L331: 1. Generate Doxygen API documentation (TODO.md DOC-003)
  - L332: 2. Create architecture diagrams (TODO.md DOC-002)
  - L333: 3. Add implementation status to all specs (TODO.md DOC-001)
  - ... 8 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FK_PHASE_A_COMPLETE_2025-11-14.md
- Potential ambiguity markers detected:
  - L49: - Supports optional column list syntax
  - L131: │ - Parse optional column list (col1, col2, ...)       │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FK_PHASE_A_ENFORCEMENT_COMPLETE_2025-11-14.md
- Potential ambiguity markers detected:
  - L28: - Both had TODO markers saying "When FK catalog is fully implemented"
  - L149: - Removed TODO marker
  - L155: - Removed TODO marker
  - L337: - **Code Changed**: 40 lines uncommented, 42 lines removed (comments/TODO)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FK_PHASE_B_COMPLETE_2025-11-14.md
- Potential ambiguity markers detected:
  - L61: - TODO marker for complex default expressions (bytecode evaluation)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FK_PHASE_C_COMPLETE_2025-11-14.md
- Potential ambiguity markers detected:
  - L64: - `CONSTRAINT` - Optional constraint name
  - L71: -- Optional constraint name
  - L96: Constraint name (string_id, optional)
  - L127: - Reads optional constraint name
  - L349: **Current Implementation**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FOREIGN_KEY_FRAMEWORK_COMPLETE_2025-11-12.md
- Potential ambiguity markers detected:
  - L218: ### INSERT into Child Table (Current Implementation Ready)
  - L247: ### UPDATE Child Table (Current Implementation Ready)
  - L364: **Current Implementation**: MATCH SIMPLE (default)
  - L407: **Current Implementation**:
  - L513: - Auto-create indexes on FK columns (optional)
  - L587: DELETE FROM customers WHERE id = 1;   -- Should cascade to orders
  - L589: SELECT * FROM orders;  -- Should be empty
  - L599: DELETE FROM customers WHERE id = 2;   -- Should set orders2.customer_id = NULL
  - L601: SELECT * FROM orders2;  -- Should show (2, NULL)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FTS_COMPLETION_REPORT_2025-11-03.md
- Potential ambiguity markers detected:
  - L86: static std::optional<TSVector> fromString(const std::string& str);
  - L87: static std::optional<TSVector> fromBinary(const std::vector<uint8_t>& data);
  - L126: static std::optional<TSQuery> fromString(const std::string& str);
  - L127: static std::optional<TSQuery> fromBinary(const std::vector<uint8_t>& data);
  - L217: -> std::optional<TSVector>;
  - L228: -> std::optional<TSQuery>;
  - L238: -> std::optional<TSQuery>;
  - L248: -> std::optional<TSQuery>;
  - L752: ✅ **Optional Returns**: All parsing functions return std::optional
  - L762: ✅ All public APIs return Status or std::optional
  - L917: - Parse 1M documents: TBD
  - L918: - Execute 10K queries: TBD
  - L919: - GIN index build time: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/FUNCTION_IMPLEMENTATION_ANALYSIS.md
- Potential ambiguity markers detected:
  - L241: - Value class may not have MultiGeometry/GeometryCollection types
  - L252: Pattern: SELECT array[1] FROM table; -- Should return first element
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/GIN_COMPLETION_REPORT_2025-11-03.md
- Potential ambiguity markers detected:
  - L209: **TODO**:
  - L219: **TODO**: Build BK-tree for O(log n) distance queries
  - L221: **Workaround**: Current implementation is correct but O(n) for large indexes
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/GIST_COMPLETION_REPORT_2025-11-03.md
- Potential ambiguity markers detected:
  - L49: - `compress()` / `decompress()` - Optional compression
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/HNSW_COMPLETION_REPORT_2025-11-03.md
- Potential ambiguity markers detected:
  - L592: ✅ Null pointer checks for optional parameters
  - L625: ✅ TODO comments for Phase 2 features
  - L756: 1. **VectorValue API:** Required careful handling of optional returns from `getFloat32Vector()`
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/HNSW_COMPLETION_REPORT_2025-11-04.md
- Potential ambiguity markers detected:
  - L106: - **TODO**: Future enhancement with diversity-based selection
  - L261: - May reduce recall in some cases
  - L328: 2. Optional: Write unit tests for API methods
  - L329: 3. Optional: Performance benchmarking against other vector databases
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/HNSW_INDEX_IMPLEMENTATION_SUMMARY.md
- Potential ambiguity markers detected:
  - L77: - **Note**: Simple distance heuristic; TODO for diversity-based selection
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/IMPLEMENTATION_TIMELINE.md
- Potential ambiguity markers detected:
  - L423: - **Lesson**: Each phase should have clear acceptance criteria
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/INDEX_AUDIT_DETAILED_FINDINGS_20251120.md
- Potential ambiguity markers detected:
  - L118: | insert() | Line 45 | ❌ STUB | `// TODO: Implement R-Tree insertion algorithm` |
  - L119: | search() | Line 52 | ❌ STUB | `// TODO: Implement R-Tree spatial search` |
  - L120: | remove() | Line 63 | ❌ STUB | `// TODO: Implement MGA logical deletion` |
  - L121: | vacuum() | Line 69 | ❌ STUB | `// TODO: Implement vacuum` |
  - L122: | removeDeadEntries() | Line 78 | ❌ STUB | `// TODO: Implement garbage collection` |
  - L200: | **Line 825** | `// TODO: Deserialize vector and compute distance` | Distance metric not implemented |
  - L201: | **Line 891** | `// TODO: Compute distance` | Search distance calculation missing |
  - L202: | **Line 1444** | `// TODO: Implement more sophisticated heuristic` | Layer selection incomplete |
  - L228: **VERDICT**: Previous audit claim "BRIN missing remove() and search()" is **FALSE**. Methods present, though may need verification.
  - L262: | insertColumn() | Line 48 | ❌ STUB | `// TODO: Implement column insertion with compression` |
  - L263: | scanColumn() | Line 55 | ❌ STUB | `// TODO: Implement column scan` |
  - L264: | vacuum() | Line 65 | ❌ STUB | `// TODO: Implement vacuum` |
  - L265: | removeDeadEntries() | Line 74 | ❌ STUB | `// TODO: Implement garbage collection` |
  - L313: - R-Tree: 100% stubbed (5/5 methods TODO)
  - L314: - Columnstore: 100% stubbed (4/4 methods TODO)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/INDEX_AUDIT_REPORT_20251120.md
- Potential ambiguity markers detected:
  - L110: insert()          - TODO (line 45)  ❌ STUB
  - L111: search()          - TODO (line 52)  ❌ STUB
  - L112: remove()          - TODO (line 63)  ❌ STUB
  - L113: vacuum()          - TODO (line 69)  ❌ STUB
  - L114: removeDeadEntries()- TODO (line 78) ❌ STUB
  - L233: - Distance computation has TODOs (line 825: "TODO: Deserialize vector and compute distance")
  - L312: insertColumn()       - TODO (line 48) ❌ STUB
  - L313: scanColumn()         - TODO (line 55) ❌ STUB
  - L314: vacuum()             - TODO (line 65) ❌ STUB
  - L315: removeDeadEntries()  - TODO (line 74) ❌ STUB
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/INDEX_REVIEW_FILES_REFERENCE.md
- Potential ambiguity markers detected:
  - L88: ❌ Real-time todo list state
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/LSM_TREE_COMPLETION_REPORT_2025-11-05.md
- Potential ambiguity markers detected:
  - L60: - **Compression support** (optional)
  - L474: 5. **WAL Integration**: Optional write-ahead logging (currently not needed for MGA)
  - L506: // Manual flush (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/MATHEMATICAL_FUNCTIONS_COMPLETE_2025-11-12.md
- Potential ambiguity markers detected:
  - L61: - ROUND and TRUNC support optional precision parameter
  - L371: ### Future Enhancements (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/MGA_ALPHA_STATUS.md
- Potential ambiguity markers detected:
  - L268: - `test_hint_bits.cpp` (unknown issues)
  - L564: ⚠️ **OPTIONAL ENHANCEMENTS**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/MIGRATION_SUMMARY_2025_10_03.md
- Potential ambiguity markers detected:
  - L77: ├── status/                           # Current implementation status
  - L93: │   └── TODO.md
  - L142: 5. ⏳ Optional: Remove `project/` directory after verification period
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/P3_HNSW_DISTANCE_METRICS_COMPLETE_2025-11-06.md
- Potential ambiguity markers detected:
  - L63: auto VectorValue::euclideanDistance(const VectorValue& other) const -> std::optional<double> {
  - L92: auto VectorValue::cosineSimilarity(const VectorValue& other) const -> std::optional<double> {
  - L104: return 0.0;  // Undefined, return 0
  - L114: - Zero vectors return 0.0 (undefined case handled gracefully)
  - L127: auto VectorValue::manhattanDistance(const VectorValue& other) const -> std::optional<double> {
  - L155: auto VectorValue::dotProduct(const VectorValue& other) const -> std::optional<double> {
  - L186: -> std::optional<double> {
  - L236: // TODO: Implement cosine distance
  - L240: // TODO: Implement Manhattan distance
  - L244: // TODO: Implement dot product distance
  - L336: - ✅ No TODO comments remaining (none were actually present)
  - L350: ### Optional: Add Test Suite to Build
  - L376: 1. **Verify before planning**: Always check current implementation before estimating work
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE1_TASK1_2_ARCHITECTURAL_NOTE.md
- Potential ambiguity markers detected:
  - L91: - Defeats purpose of index (should return TIDs, not fetch data)
  - L96: - Our implementation should follow Firebird's proven design
  - L175: What we did NOT do (and should not do):
  - L187: - Misunderstanding of where visibility should be checked
  - L206: - [ ] **1.2.6**: Add MVCC tests - TODO (but lower priority, existing code already works)
  - L244: 4. **No bugs**: Current implementation already handles MVCC correctly
  - L280: The original plan's assumption that visibility filtering should happen IN the index layer was incorrect for Firebird MGA architecture. The correct approach (which is already implemented) is:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE2_GC_COMPLETION_SUMMARY.md
- Potential ambiguity markers detected:
  - L424: 4. **Hypothesis**: Other index types may also be complete
  - L438: **Minor Areas for Enhancement** (optional):
  - L451: ### Optional Follow-Up Work
  - L479: **Hypothesis**: Based on pattern (all Phase 2 tasks complete), TASK 2.6 may also be done.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE4_NEW_INDEX_TYPES_DEPENDENCY_ANALYSIS.md
- Potential ambiguity markers detected:
  - L557: ✅ **YES - BRIN and VECTOR can and should be implemented**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_1_5_TID_MIGRATION_STATUS.md
- Potential ambiguity markers detected:
  - L143: **On-Disk Format**: Hash buckets store TIDs, may need format update
  - L151: // Posting lists store TIDs - on-disk format may need update
  - L161: - **Critical**: Posting tree TID storage (on-disk) may need format update
  - L162: - **Critical**: Posting list compression may be TID-based
  - L340: - May need to rebuild all indexes instead of converting in-place
  - L692: **Should we complete Phase 1.5 now (Option A) or defer (Option B)?**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_2_COMPLETION_PLAN.md
- Potential ambiguity markers detected:
  - L150: -- Should show "Index Scan using idx_stores_location"
  - L429: RAISE EXCEPTION 'Unknown state: %', state;
  - L516: - Risk: Implementation may not match PostGIS performance
  - L533: - Risk: Generated code may have bugs
  - L538: - Risk: New features may break existing code
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_2_STATUS_OCT_28_2025.md
- Potential ambiguity markers detected:
  - L386: - **R-tree performance**: May not match PostGIS initially
  - L399: - **Agent code quality**: Generated code may have bugs
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/PHASE_3_0_PLANNING_2025-11-11.md
- Potential ambiguity markers detected:
  - L156: 4. SET ROLE, SELECT from table → should work
  - L158: 6. SELECT from table → should fail
  - L159: 7. DROP USER CASCADE → should remove all memberships
  - L197: | 3.0.1 Password Hashing | 2-3h | TBD | Includes bcrypt setup |
  - L198: | 3.0.2 Test API | 10m | TBD | Simple find/replace |
  - L199: | 3.0.3 ALTER USER | 1h | TBD | Catalog API extension |
  - L200: | 3.0.4 checkPermission | 3-4h | TBD | Full permission hierarchy |
  - L201: | 3.0.5 CASCADE | 5-8h | TBD | Most complex task |
  - L202: | 3.0.6 Documentation | 1h | TBD | Update 3 docs |
  - L203: | **Total** | **12-18h** | **TBD** | **~2-3 days** |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_IMPLEMENTATION_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L40: **Purpose**: Determine if RLS should be enforced for current user
  - L43: - Table owners bypass RLS (unless FORCE RLS) [TODO]
  - L53: // TODO: Check table's FORCE RLS flag
  - L57: // TODO: Check if user is table owner
  - L113: - TODO: Resolve role names to IDs
  - L122: // TODO: Resolve role names to IDs and check membership
  - L162: 3. TODO: Set up row context (column references)
  - L372: // TODO: Set up row context so COLUMN_REF opcodes resolve to row_values
  - L378: // TODO: Query catalog for table.rls_forced
  - L384: // TODO: Compare current_user_id with table.owner_id
  - L390: // TODO: Get username from user_id, role name from role_id
  - L391: // TODO: Check membership in policy.roles vector
  - L413: 5. Fix TODO #1: Row context binding in `evaluatePolicyExpression()` (2-3 hours)
  - L414: 6. Fix TODO #2: FORCE RLS flag checking (30 min)
  - L415: 7. Fix TODO #3: Table ownership checking (1 hour)
  - L421: 9. Fix TODO #4: Role membership resolution (2-3 hours)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_IMPLEMENTATION_PLAN_UPDATE_2025-11-10.md
- Potential ambiguity markers detected:
  - L106: - **Phase 3.5:** Policy-Based Access (8-12 hours - optional) - Same
  - L168: ### Week 4 (OPTIONAL)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_BYTECODE_COMPLETE_2025-11-10.md
- Potential ambiguity markers detected:
  - L171: **Why**: Main opcode space (0x00-0xFF) is valuable and should be reserved for frequently-used core operations. Security statements are less frequent.
  - L178: **Why**: Many security statements have optional boolean flags (IF EXISTS, CASCADE, WITH GRANT OPTION, etc.)
  - L190: **Why**: Usernames, role names, object names are strings that may be repeated.
  - L434: **Next recommended step**: Implement executor handlers for security opcodes (Tasks 9-10 in todo list).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_COMPLETE_2025-11-10.md
- Potential ambiguity markers detected:
  - L58: | `executeAlterUser` | Uses UserInfo struct, preserves existing values, TODO for superuser flag | ✅ Complete |
  - L92: // TODO: Get current user ID from connection context
  - L98: // In production, this should:
  - L199: - Extensively documented with TODO comments
  - L248: - DROP/ALTER should check ownership, but reuse existing privilege constants
  - L249: - In production, ownership checks should be added separately
  - L291: - Add password strength validation (optional)
  - L295: - May need new `src/core/password_hash.cpp` module
  - L388: 9. **executeCreateGroup:** Default to LOCAL type, may need GROUP TYPE in parser
  - L436: SELECT * FROM restricted_table;  -- Should fail
  - L437: INSERT INTO restricted_table VALUES (...);  -- Should fail
  - L441: -- Now should succeed
  - L442: SELECT * FROM restricted_table;  -- Should succeed
  - L455: SELECT * FROM users;  -- Should succeed
  - L456: DELETE FROM users WHERE id = 1;  -- Should fail (no DELETE privilege)
  - L592: -- 2. Grant PUBLIC access to existing objects (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L247: - Status: TODO comment in checkPermission()
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_EXECUTOR_STARTED_2025-11-10.md
- Potential ambiguity markers detected:
  - L80: // This should be stored in the Executor class
  - L108: // TODO: Implement proper password hashing
  - L714: # Should be added before closing namespace braces (line 12382-12383)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_FINAL_STATUS_2025-11-10.md
- Potential ambiguity markers detected:
  - L91: - Handles optional clauses
  - L100: - Handles optional IF EXISTS and CASCADE/RESTRICT
  - L154: - Shows all optional clauses
  - L240: // TODO: Implement bytecode generation for CREATE USER
  - L294: - Test optional clause parsing
  - L360: - Defaults to USER if not specified
  - L450: 3. Supports all optional SQL clauses
  - L461: - **Impact**: Low - current implementation works, just not elegant
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_PARSER_COMPLETE_2025-11-10.md
- Potential ambiguity markers detected:
  - L67: - `parseCreateUser()` - Parses CREATE USER with optional PASSWORD and SUPERUSER
  - L68: - `parseAlterUser()` - Parses ALTER USER with optional PASSWORD and SUPERUSER
  - L69: - `parseDropUser()` - Parses DROP USER with optional IF EXISTS and CASCADE/RESTRICT
  - L71: - `parseDropRole()` - Parses DROP ROLE with optional IF EXISTS and CASCADE/RESTRICT
  - L73: - `parseDropGroup()` - Parses DROP GROUP with optional IF EXISTS and CASCADE/RESTRICT
  - L252: - **Semantic Level**: Type mismatches, undefined names (minimal validation for security stmts)
  - L310: **Next recommended step**: Implement bytecode opcodes and generation (Tasks 7-8 in todo list).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_PROGRESS_2025-11-10.md
- Potential ambiguity markers detected:
  - L208: // Optional: WITH PASSWORD 'password'
  - L219: // Optional: SUPERUSER | NOSUPERUSER
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE2_SESSION_SUMMARY_2025-11-10.md
- Potential ambiguity markers detected:
  - L226: - `include/scratchbird/core/connection_context.h` - May need session fields
  - L238: - May need new `src/core/password_hash.cpp` module
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_0_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L215: - Deletes all group mappings (TODO)
  - L366: 3. **Group Mappings**: CASCADE doesn't clean up group mappings (TODO marker added)
  - L428: 4. (Optional) Begin Phase 3.1: External Authentication
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_1_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L44: - TODO: Role/group membership resolution (deferred to Phase 3.2)
  - L112: - May need new EXT_CALL_PROCEDURE and EXT_CALL_FUNCTION opcodes
  - L321: 4. **Cascade Revoke**: REVOKE does not yet cascade to WITH GRANT OPTION delegations. This should be implemented in Phase 3.2.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_2_1_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L117: - Optional parameter to `planQuery()`
  - L292: - JOINs need testing (should work via recursive planning)
  - L298: ### Immediate (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_2_2_ANALYSIS_2025-11-11.md
- Potential ambiguity markers detected:
  - L167: **Current implementation is 10-15x faster than per-row checking!**
  - L185: **DML (Current Implementation)**:
  - L204: - Document the current implementation
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_2_3_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L179: 3. Re-check entry (another thread may have removed it)
  - L274: The following tests should be written in a future session:
  - L307: -- Should show: hit_rate ~95%, current_entries ~50, etc.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_2_3_PARTIAL_2025-11-11.md
- Potential ambiguity markers detected:
  - L61: std::optional<bool> lookup(const CacheKey& key);
  - L64: // - std::optional<bool> with has_permission if cached and not expired
  - L461: When Phase 3.2.3 is complete, we should achieve:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_1_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L255: // Should print non-zero page number (e.g., page 50-60 depending on catalog size)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_2_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L120: - ✅ TODO marker for role/group permissions (Phase 3.3.3)
  - L401: **TODO**: Check role memberships and group memberships
  - L405: // TODO: Check role memberships and group memberships (Phase 3.3.3)
  - L417: **TODO**: Integrate with global permission cache from Phase 3.2.3
  - L428: **TODO**: Use BFS transitive closure from Phase 3.0
  - L433: -- If senior_developer_role has SELECT on salary, alice should too
  - L452: -- Parser should recognize column lists in parentheses
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_3_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L84: // Security Phase 3.3.3: Parse optional column list (col1, col2, ...)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_4_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L316: **Optimization**: Could batch column writes in future, but current implementation prioritizes correctness.
  - L413: -- Should see:
  - L419: -- Query pg_column_permissions should show 2 records
  - L423: SELECT id, name FROM employees;     -- Should succeed
  - L424: SELECT salary FROM employees;       -- Should fail: Permission denied
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_5_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L527: // Should work
  - L531: // Should fail
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_3_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L526: 3. **No GRANT OPTION for Columns**: WITH GRANT OPTION works but grantees can't re-grant column permissions (limitation of current implementation)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_1_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L59: - **with_check_expr**: Optional (empty for SELECT/DELETE policies)
  - L98: with_check_expr TEXT,              -- SQL expression (optional)
  - L170: ### Optional WITH CHECK Clause
  - L171: **Decision**: with_check_expr is optional (can be empty string)
  - L186: | ALL         | ✅ Required | ✅ Optional |
  - L330: - **Caching**: Policies should be cached in memory (future optimization)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_2_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L78: **TOAST Status**: TODO markers added for roles and expressions (OIDs set to 0)
  - L148: - TODO: Filter by user roles (placeholder for Phase 3.4.5)
  - L150: **Current Implementation**: Returns all table policies (role filtering deferred)
  - L276: **Decision**: Mark TOAST integration as TODO, set OIDs to 0 for now
  - L278: **Current Implementation**:
  - L280: // TODO: TOAST integration for roles, using_expr, with_check_expr
  - L544: - ✅ PolicyRecord structure with TOAST support (TODO markers)
  - L556: - Quality: Production-ready (with TOAST TODO)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_3_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L136: // SemanticAnalyzer implementation (with TODO markers)
  - L139: // TODO Phase 3.4.3: Add full semantic validation
  - L148: // TODO Phase 3.4.3: Add semantic validation
  - L156: // TODO Phase 3.4.3: Add semantic validation
  - L204: - Optional FOR clause (defaults to ALL)
  - L205: - Optional TO clause with comma-separated role list or PUBLIC
  - L206: - Optional USING clause with expression parsing
  - L207: - Optional WITH CHECK clause with expression parsing
  - L241: - Optional IF EXISTS clause
  - L243: - Optional CASCADE or RESTRICT (defaults to RESTRICT)
  - L395: **Decision**: Store Expression* (can be nullptr) instead of optional<Expression>
  - L400: - No need for std::optional overhead
  - L461: ## Semantic Validation (TODO)
  - L463: Current implementation has placeholder stubs. Full validation needs:
  - L476: - ALL: requires USING, optional WITH CHECK
  - L572: **Current**: Stub implementations with TODO markers
  - L579: **Impact**: May parse invalid expressions for policy context
  - L587: **Current**: Policy expressions may reference non-existent columns
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_4_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L163: - Table owner (TODO: ownership check not yet implemented)
  - L167: - Table owner (TODO: ownership check not yet implemented)
  - L171: - Table owner (TODO: ownership check not yet implemented)
  - L204: **Current Workaround**: TODOs in code mark where ownership check should be added.
  - L287: - ✅ TODO markers for future work
  - L460: - ⚠️ Table ownership checks marked as TODO
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_5_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L35: - Returns true if RLS should be enforced, false otherwise
  - L85: 4. Marks TODO for Phase 3.4.6 (predicate injection)
  - L104: // TODO Phase 3.4.6: Apply policy predicates to WHERE clause
  - L195: **Fix**: Phase 3.4.4 marked expression handling as TODO. Need to:
  - L379: - ⚠️ Expression storage not implemented (Phase 3.4.4 TODO)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_6_DEFERRED_2025-11-11.md
- Potential ambiguity markers detected:
  - L15: Phase 3.4.6 (Executor DML Integration) has been **deferred** pending the implementation of expression storage in TOAST. The current implementation has all the structural pieces in place for RLS, but cannot evaluate policy expressions because they are not yet persisted to or loaded from the catalog.
  - L31: - ❌ Expressions are NOT serialized to TOAST (marked as TODO)
  - L41: // TODO: Store using_expr in TOAST and save OID
  - L46: // TODO: Store with_check_expr in TOAST and save OID
  - L54: // TODO: Load using_expr from TOAST using using_expr_oid
  - L57: // TODO: Load with_check_expr from TOAST using with_check_expr_oid
  - L165: SELECT * FROM documents;  -- Should filter by tenant_id
  - L191: INSERT INTO documents VALUES (...);  -- Should validate status
  - L294: - "Complete but not functional" may confuse users
  - L353: SELECT * FROM table1;  -- Should fail
  - L363: SELECT * FROM table1;  -- Should succeed
  - L373: SELECT * FROM table1;  -- Should fail
  - L479: 1. **Expression Storage Scoping**: Should have tackled TOAST earlier
  - L481: 3. **Testing Strategy**: Should have identified testable subset earlier
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_6_EXPRESSION_STORAGE_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L83: uint64_t xmin = 1;  // TODO: Get from transaction context in future
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_7_RUNTIME_EVALUATION_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L128: Replaced TODO comment with full implementation:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_8_TOAST_PERSISTENCE_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L184: uint64_t xmin = 1;  // TODO: Get from transaction context
  - L221: uint64_t xmax = 1;  // TODO: Get from transaction context
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_4_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L324: // TODO: Read and evaluate expression bytecode
  - L442: - Unknown user = deny all ✅
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_5_COMPLETE_2025-11-12.md
- Potential ambiguity markers detected:
  - L87: - `src/sblr/executor.cpp` (~44 lines modified, was TODO stub)
  - L111: - PolicyInfo.roles currently stores NAMES (should migrate to UUIDs)
  - L112: - Transitive role membership not yet implemented (TODO)
  - L115: - `src/sblr/executor.cpp` (~57 lines, was TODO placeholder)
  - L120: - Constructs full row_values with defaults for unspecified columns
  - L297: - **policyAppliesToUser()**: O(r) where r = roles per policy (currently, should be O(1) with UUID lookup)
  - L310: 1. **PolicyInfo.roles** stores role NAMES (strings), should migrate to role IDs (UUIDs)
  - L316: - TODO: Check roles inherited from groups via recursive BFS
  - L321: - TODO: Generate actual SBLR bytecode for policy expressions
  - L325: - TODO: Create `test_security_phase3_1_object_permissions.cpp`
  - L372: - TODO: Generate actual bytecode for policy expressions
  - L397: ### Immediate (Optional Polish):
  - L427: - **UUID-based identity** throughout (except PolicyInfo.roles - TODO)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_SESSION_COMPLETE_2025-11-11.md
- Potential ambiguity markers detected:
  - L223: - [ ] Password hashing benchmark (should be ~250ms at cost 12)
  - L267: ### Immediate (Optional):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_PHASE3_STATUS_2025-11-11_FINAL.md
- Potential ambiguity markers detected:
  - L63: - TODO: Role/group membership expansion
  - L114: - Parser handles optional SQL SECURITY clause
  - L171: - May need EXT_CALL_PROCEDURE and EXT_CALL_FUNCTION opcodes
  - L525: - **Optimization**: Cache compiled expressions (TODO)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SECURITY_SESSION_2025-11-11.md
- Potential ambiguity markers detected:
  - L62: // Parse SQL SECURITY clause (Phase 3.1 - optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SEQUENCES_IMPLEMENTATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L57: - `CreateSequenceStmt` - Optional parameters (increment, min, max, start, cache, cycle)
  - L64: - `parseCreateSequence()` - Full syntax with optional parameters
  - L76: - `visit(CreateSequenceStmt*)` - Encodes optional parameters with flag bytes
  - L265: - Reads name and all optional parameters
  - L266: - **TODO**: Look up sequence ID by name, call `catalog_manager->alterSequence()`
  - L270: - **TODO**: Look up sequence ID by name, call `catalog_manager->dropSequence()`
  - L274: - **TODO**: Look up ID, call `sequenceNextVal()`, store in `session_sequence_currval_[id]`
  - L278: - **TODO**: Look up ID, check `session_sequence_currval_[id]`, error if not found
  - L282: - **TODO**: Look up ID, call `sequenceSetVal()`
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-06_COMPILATION_SUCCESS.md
- Potential ambiguity markers detected:
  - L384: 4. **Performance** (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_2_1.md
- Potential ambiguity markers detected:
  - L177: - Optional `conn_ctx` parameter (defaults to nullptr)
  - L242: - Should work via recursive planning
  - L289: ### Immediate (Optional)
  - L330: 1. **Clean API Design** - ConnectionContext as optional parameter works perfectly
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_2_3.md
- Potential ambiguity markers detected:
  - L234: The following tests should be written in a future session:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_3_COMPLETE.md
- Potential ambiguity markers detected:
  - L294: 3. No GRANT OPTION re-granting for columns (limitation of current implementation)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_3_PROGRESS.md
- Potential ambiguity markers detected:
  - L127: | 3.3.5 - Column Filtering | ⏭️ Pending | ~3-5h | TBD |
  - L128: | 3.3.6 - Testing | ⏭️ Pending | ~2-3h | TBD |
  - L140: **Challenge**: Need to parse optional column lists after each privilege keyword
  - L148: // Security Phase 3.3.4: Parse optional column list
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_2025-11-11_PHASE3_4_STARTED.md
- Potential ambiguity markers detected:
  - L63: - TOAST integration marked as TODO for future work
  - L205: ### 4. Optional WITH CHECK
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_FINAL_2025-11-11.md
- Potential ambiguity markers detected:
  - L281: 4. Optional: Run manual tests of column-level permissions
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_SUMMARY_2025-11-06.md
- Potential ambiguity markers detected:
  - L155: - `Status::ERROR` doesn't exist (should be `Status::IO_ERROR`)
  - L291: - Should meet targets
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_SUMMARY_2025-11-07.md
- Potential ambiguity markers detected:
  - L62: - Integration tests (2-4 hours) - Optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SESSION_SUMMARY_2025_11_03.md
- Potential ambiguity markers detected:
  - L39: - Indexes should be TOAST-unaware (separation of concerns)
  - L243: | **Committed Data** | May be in WAL only | Always on disk |
  - L258: - Storage layer should handle detoasting
  - L506: Indexes should not know about TOAST. Storage layer should handle all TOAST complexity.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/SRID_VALIDATION_CLARIFICATION.md
- Potential ambiguity markers detected:
  - L20: -- This should produce a warning/error at planning time:
  - L151: - ⚠️ **Result may be incorrect** (comparing incompatible coordinate systems)
  - L226: -> std::optional<int32_t>
  - L249: return std::nullopt;  // SRID unknown at planning time
  - L318: Users should be advised:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_CURRENT_STATUS.md
- Potential ambiguity markers detected:
  - L199: ### Current Implementation
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_EXECUTIVE_SUMMARY.md
- Potential ambiguity markers detected:
  - L61: - May index uncommitted data
  - L62: - May return wrong results under concurrent access
  - L63: - May violate isolation guarantees
  - L64: - May cause data corruption on rollback
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_FINAL_SESSION_SUMMARY.md
- Potential ambiguity markers detected:
  - L117: The current implementation:
  - L130: 1. May index uncommitted data
  - L131: 2. May return wrong results under concurrent access
  - L132: 3. May violate isolation guarantees
  - L133: 4. May cause data corruption on rollback
  - L134: 5. May crash due to missing versions
  - L341: 2. ⚠️ **Fix Remaining Test Failures** (Optional - 6-10 hours)
  - L346: 3. 📝 **Phase 13 Documentation** (Optional - 8-12 hours)
  - L381: 1. ⚠️ **MGA awareness from start** - Should have reviewed MGA architecture first
  - L382: 2. ⚠️ **Test-driven development** - Tests should have been written earlier
  - L383: 3. ⚠️ **Cross-parser test design** - Should have used shared StringPool from start
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_COMPLIANCE_ANALYSIS.md
- Potential ambiguity markers detected:
  - L15: Task 17 (Expression and Filtered Indexes) was implemented **without proper MGA integration**. The current implementation:
  - L23: **Impact**: Expression and filtered indexes are **NOT transaction-safe** and may:
  - L58: ## Task 17 Current Implementation Analysis
  - L92: 1. **No transaction context passed to scan** - Should pass current TransactionId
  - L93: 2. **No visibility check** - Should use `sb_check_visibility()`
  - L94: 3. **Indexes all versions** - Should only index visible, committed rows
  - L95: 4. **Ignores isolation level** - Should respect transaction's isolation level
  - L218: **Current Implementation**:
  - L227: 3. May evaluate expressions on uncommitted changes
  - L282: **Current Implementation**:
  - L285: - Which predicates should be evaluated at what snapshot
  - L289: Query planner may use filtered index that's only correct for certain isolation levels.
  - L301: -- Session 1 should NOT see new row (SERIALIZABLE)
  - L302: -- But filtered index may include it if evaluated at READ_COMMITTED!
  - L319: **Current Implementation**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_COMPLIANCE_COMPLETE_SUMMARY.md
- Potential ambiguity markers detected:
  - L9: **Verdict**: Current implementation is **CORRECT for MGA** - No bugs found!
  - L52: ## Current Implementation Review
  - L148: #### 2.1 Optional Debug Logging (2-3 hours)
  - L336: - Current implementation: one entry per key per TID
  - L412: - Index may have entries from aborted transactions
  - L413: - Index may have entries for deleted tuples
  - L434: 1. Debug logging (optional, off by default)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_PHASE_1_3_ASSESSMENT.md
- Potential ambiguity markers detected:
  - L28: Snapshot support should be implemented when **query execution code** uses expression/filtered indexes for SELECT operations, not in the index maintenance code.
  - L86: ### 3. Task 17 Current Implementation
  - L263: | **Should Phase 1.3 be implemented now?** | ❌ NO | Wait for query execution implementation |
  - L329: // Double-check visibility (snapshot may be stale)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_PHASE_2_2_PARTIAL.md
- Potential ambiguity markers detected:
  - L148: → May indicate high transaction contention
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_ROLLBACK_ANALYSIS.md
- Potential ambiguity markers detected:
  - L286: - Optional debug logging for monitoring
  - L308: - **Eventual consistency**: Index cleanup may lag heap cleanup (acceptable)
  - L424: ### 7.1 What Phase 2 Should Be
  - L429: 1. Add optional debug logging for monitoring (2-3 hours)
  - L446: ### 7.2 What Phase 2 Should NOT Be
  - L557: - GC integration ⏳ TODO (Phase 2)
  - L558: - Visibility-aware scans ⏳ TODO (Phase 3)
  - L632: > 3. **Eventual Consistency**: Index cleanup may lag heap cleanup (acceptable)
  - L633: > 4. **Non-Blocking**: Index GC should not block normal index operations excessively
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_MGA_SESSION_2_SUMMARY.md
- Potential ambiguity markers detected:
  - L21: - ✅ Phase 2.1 complete (optional debug logging)
  - L71: - Optional debug logging (monitoring)
  - L89: **Current implementation is CORRECT for MGA Phase 1**:
  - L110: **What**: Optional debug logging for all index operations
  - L330: **Current implementation**:
  - L503: - ✅ Phase 2.1 complete (optional debug logging)
  - L538: 2. `a0271f4` - Task 17 MGA Phase 2.1 COMPLETE: Optional Debug Logging
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_PHASE_10_12_COMPLETION_REPORT.md
- Potential ambiguity markers detected:
  - L104: - `NoMatchDifferentIdentifiers` - "email" vs "name" should not match
  - L105: - `NoMatchDifferentFunctions` - "LOWER" vs "UPPER" should not match
  - L122: - `RealWorldExtractYearIndex` - May not support EXTRACT() function
  - L230: 1. ❌ May index uncommitted data
  - L231: 2. ❌ May return wrong results under concurrent access
  - L232: 3. ❌ May violate isolation guarantees
  - L233: 4. ❌ May cause data corruption on rollback
  - L308: **Deferred Reason**: Should complete after MGA compliance fixes to ensure documented behavior is correct.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_PHASE_10_12_STATUS.md
- Potential ambiguity markers detected:
  - L255: - May uncover additional API mismatches
  - L257: - Integration tests may reveal more issues
  - L307: 2. `src/sblr/expression_evaluator.cpp` - Unknown status (may have similar issues)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_PHASE_1_SERIALIZER_FIXES.md
- Potential ambiguity markers detected:
  - L85: // ERROR: value.toInt() should be value.toInt64()
  - L89: // ERROR: value.toBool() should be value.toBoolean()
  - L93: // ERROR: value.getBool() should be value.getBoolean()
  - L118: // ERROR: functionName() should be name()
  - L122: // ERROR: arguments() should be args()
  - L129: // ERROR: expression() should be expr()
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_QUICK_REFERENCE.md
- Potential ambiguity markers detected:
  - L66: - ❌ Concurrent transactions (may corrupt data)
  - L185: # BAD - may return wrong results
  - L239: Using in multi-user environments may cause:
  - L264: # Check for MGA usage (should find NONE currently)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_17_SESSION_REPORT.md
- Potential ambiguity markers detected:
  - L98: 7. `RealWorldExtractYearIndex` - Parser may not support EXTRACT
  - L100: **Root Cause**: Likely StringPool issue - expressions from different parsers may have overlapping StringIds
  - L133: - `NoMatchDifferentIdentifiers` - "email" vs "name" should not match, but StringIds might overlap
  - L153: - `RealWorldExtractYearIndex` - May not support EXTRACT() function
  - L198: - Should fix ~10-12 failures
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_2_RTREE_PLANNER_PROGRESS.md
- Potential ambiguity markers detected:
  - L375: // TODO: Set spatial_cond string for EXPLAIN
  - L509: -- Should warn about SRID mismatch OR successfully transform
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_4_MULTI_GEOMETRY_STATUS.md
- Potential ambiguity markers detected:
  - L88: **Phase 2 Scope (Optional - can defer to Phase 3)**:
  - L132: - Time pressure may compromise testing
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_5_IMPLEMENTATION_GUIDE.md
- Potential ambiguity markers detected:
  - L430: "Spatial predicate %s may have SRID mismatch. "
  - L471: - 40+ tests should pass
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_5_S3_COMPLETION_REPORT.md
- Potential ambiguity markers detected:
  - L40: int32_t srid;  // NEW: Spatial Reference Identifier (0 = undefined)
  - L57: - **Backward Compatible**: Default SRID = 0 (undefined) matches previous behavior
  - L294: 1. SRID = 0 means "undefined" (backward compatible)
  - L426: - Add SRID column to spatial_ref_sys catalog table (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_9_AGENT_STRATEGY.md
- Potential ambiguity markers detected:
  - L316: ./tests/wave1_tests  # All 44 spatial tests should still pass
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/TASK_DML_1_GIN_INDEX_DML_INTEGRATION.md
- Potential ambiguity markers detected:
  - L51: // TODO: Use specialized extractors based on indexed column type
  - L70: // TODO: Use specialized extractors based on indexed column type
  - L76: // This should be updated to use logical deletion (xmax marking) when TASK-CRITICAL-1 is done
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/VIEWS_IMPLEMENTATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L23: - ✅ Optional column name specifications
  - L99: std::vector<StringPool::StringId> column_names_;  // Optional column aliases
  - L114: - Optional column name aliases
  - L146: - Optional column name specifications
  - L293: std::vector<std::string> column_names;  // Optional column aliases
  - L509: - Reads optional column names
  - L781: - ✅ All CREATE VIEW statements should succeed
  - L782: - ✅ OR REPLACE should update existing views
  - L783: - ✅ DROP VIEW should succeed for existing views
  - L784: - ✅ DROP VIEW IF EXISTS should not error for non-existent views
  - L785: - ✅ WITH CHECK OPTION should be accepted (not enforced)
  - L786: - ✅ CASCADE/RESTRICT should be accepted
  - L931: -- Future: This should error because salary < 100000
  - L945: DROP VIEW view1 CASCADE;  -- Should drop view2 too (not implemented)
  - L1134: - May grow or shrink depending on definition
  - L1151: | OR REPLACE | ⚠️ Optional | ✅ Implemented | PostgreSQL extension |
  - L1153: | IF EXISTS | ⚠️ Optional | ✅ Implemented | Common extension |
  - L1157: | Updatable views | ⚠️ Optional | ❌ Deferred | Future phase |
  - L1260: The Views implementation is complete for all query use cases. The next priorities should be:
  - L1406: column_names TEXT[],  -- Optional column aliases
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/VIEWS_PARSING_FIX_2025-11-08.md
- Potential ambiguity markers detected:
  - L57: 2. **Data Type Support**: BOOLEAN type causes "Unknown data type opcode" error in bytecode executor
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_1_COMPLETION_REPORT.md
- Potential ambiguity markers detected:
  - L69: // Polygon: Closed ring with optional holes
  - L72: std::vector<std::vector<Point>> interior_rings;  // Holes (optional)
  - L500: **Risk**: Wave 2 agents may encounter same test infrastructure issues
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_1_SESSION_HANDOFF.md
- Potential ambiguity markers detected:
  - L304: ./tests/wave1_tests  # Should show 44/44 spatial tests passing
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_2_COMPLETION_SUMMARY.md
- Potential ambiguity markers detected:
  - L408: - Should have been more conservative
  - L413: - Agents should compile and test as they go
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_2_PROGRESS_REPORT.md
- Potential ambiguity markers detected:
  - L394: - Agents should deliver complete layers, not partial implementations
  - L399: - Agents should compile and test as they go
  - L405: - Should be separate agent task or higher priority
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_3_AGENT_PERMISSIONS.md
- Potential ambiguity markers detected:
  - L110: // ============ VERSION CONTROL (Optional) ============
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/2025-11-completion-reports/WAVE_3_LAUNCH_PLAN.md
- Potential ambiguity markers detected:
  - L562: - Risk: May not match PostGIS
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/development/TODO.md
- Potential ambiguity markers detected:
  - L1: # Todo
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/README.md
- Potential ambiguity markers detected:
  - L40: - **Status:** `/docs/specifications/parser/v3/status/` - Current implementation status and completion reports
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/2025-09-15_btree_analysis_report.md
- Potential ambiguity markers detected:
  - L69: // TODO: Implement B-Tree insertion logic
  - L74: // TODO: Implement B-Tree search logic
  - L79: // TODO: Implement B-Tree removal logic
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/COMPRESSION_IMPLEMENTATION_SUMMARY.md
- Potential ambiguity markers detected:
  - L57: - Optional LZ4 dependency via CMake
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/HEAP_TOAST_INTEGRATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L26: - `get_tuple()` returns raw data (may contain TOAST pointer)
  - L48: 1. **Optional TOAST Support**: HeapPage works with or without ToastManager
  - L69: The current implementation is production-ready and can be used once the database infrastructure is fully initialized.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/ImplementationAndReviewProcessLog.md
- Potential ambiguity markers detected:
  - L364: ok, merge the work to the main branch -remember that the doc directory has been cleaned up but should have no issue with your changes
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/STAGE_1_1_COMPLETE_SUMMARY.md
- Potential ambiguity markers detected:
  - L48: - Optional LZ4 dependency - compiles without compression library
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_01_1.log.md
- Potential ambiguity markers detected:
  - L183: - Updated all Database methods to accept optional ErrorContext
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_03.log.md
- Potential ambiguity markers detected:
  - L82: - **Test Coverage**: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/alpha_1_05_sql_parser.log.md
- Potential ambiguity markers detected:
  - L29: ### [Date TBD] - Initial Planning
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/catalog_manager_refactoring_issues_2025-09-08.md
- Potential ambiguity markers detected:
  - L53: *   Keeping its current implementation (which is functional but not genericized) and marking the refactoring as complete for this specific function.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/implementation/AGENT_A_FINAL_FIXES_STAGE_1_1.md
- Potential ambiguity markers detected:
  - L128: The single failing test (performance regression) represents an acceptable trade-off that should be documented rather than fixed.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/implementation/AGENT_A_FIXES_STAGE_1_1_ISSUES.md
- Potential ambiguity markers detected:
  - L59: 2. **Cache Effects**: Larger pages may have worse cache locality for small operations
  - L132: **Next Steps**: Performance test expectations may need adjustment
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/implementation/AGENT_B_REVIEW_SUMMARY_STAGE_1_1_PAGE_SIZES.md
- Potential ambiguity markers detected:
  - L151: 1. Should we add runtime checks to use different structures based on page size?
  - L153: 3. Should we add warnings when creating databases with very large page sizes?
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/implementation/IMPL_PROGRESS_TEMPLATE.md
- Potential ambiguity markers detected:
  - L67: [Any limitations or TODOs in current implementation]
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-progress/stage_1_1_extended_page_sizes.log.md
- Potential ambiguity markers detected:
  - L9: ## Reviewer: TBD (Agent B)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/AGENT_C_REVIEW_TEST_SUMMARY.md
- Potential ambiguity markers detected:
  - L22: - **Result**: Confirms current implementation is safe
  - L141: All tests should pass. Key outputs:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/AGENT_C_TEST_REPORT_FOR_AGENT_A.md
- Potential ambiguity markers detected:
  - L57: - Test methodology may need adjustment
  - L97: 1. **Item Count Limits**: Current implementation safely handles up to 4,516 items in a 128KB page, well below the uint16_t limit of 65,535.
  - L103: 4. **Arithmetic Safety**: The current implementation appears to handle offset arithmetic safely, though explicit overflow checks might improve robustness.
  - L148: The Extended Page Sizes feature is largely working correctly. However, the missing page size validation in HeapPage::initialize() represents a real issue that should be addressed before the feature is considered complete. This aligns with Agent B's security review findings.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/COMPREHENSIVE_TEST_REPORT.md
- Potential ambiguity markers detected:
  - L18: **This report should be considered HISTORICAL ONLY until the build is fixed.**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/CORRECTED_ALPHA_104_ASSESSMENT.md
- Potential ambiguity markers detected:
  - L79: **Recommendation**: The implementation is adequate for Alpha phase despite the known issues. My test quality was poor and should not be used to judge the implementation.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/CRITICAL_FIXES_TEST_PLAN.md
- Potential ambiguity markers detected:
  - L8: This document describes the tests created to verify the critical fixes required for the Storage Engine implementation. These tests are designed to **FAIL** with the current implementation and **PASS** once Agent A applies the fixes.
  - L64: - **Expected**: Currently FAILS or causes undefined behavior
  - L69: - **Method**: Shows how iterator should use parent engine
  - L81: - **Expected**: Currently causes undefined behavior
  - L157: 1. Agent A should review these tests
  - L164: - These tests use `get_memory_usage()` which may need adjustment for different platforms
  - L165: - Some tests may need `valgrind` or similar tools for detailed memory leak detection
  - L166: - The buffer overflow tests may trigger AddressSanitizer if enabled
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/EDGE_CASE_TEST_REPORT.md
- Potential ambiguity markers detected:
  - L57: - May indicate issue with page persistence
  - L80: - Single-threaded design should be explicit
  - L89: - FSM state may not persist correctly
  - L110: ### Should Implement (From UNTESTABLE_REQUIREMENTS.md)
  - L139: These are all minor issues that don't block the Alpha release but should be addressed for production quality.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/FINAL_TEST_STATUS_REPORT.md
- Potential ambiguity markers detected:
  - L18: **This report should be considered HISTORICAL ONLY until the build is fixed.**
  - L104: 1. **PASS** - Correctly test the current implementation
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/LEXER_COMPREHENSIVE_TEST_PLAN.md
- Potential ambiguity markers detected:
  - L140: "9223372036854775807"  // ✅ Should parse
  - L143: "9223372036854775808"  // ❌ Should error
  - L149: "'\\n\\t\\r\\\\\\'\\"'"  // ✅ Should parse as "\n\t\r\'\""
  - L152: "'\\x'"  // ❌ Should error
  - L157: // 1MB identifier - Should complete in < 100ms
  - L160: // 100k tokens - Should maintain > 1M tokens/sec
  - L166: "SEL\u200BECT"  // Zero-width space - should error
  - L169: std::string(10'000'000, 'a')  // Should handle without hanging
  - L185: 1. **Error Recovery**: May not recover gracefully from all errors
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/UNTESTABLE_REQUIREMENTS.md
- Potential ambiguity markers detected:
  - L9: This document lists requirements and recommendations from Agent B's security review that cannot be directly tested in the unit test framework, but should still be implemented by Agent A.
  - L20: - Document that current implementation is single-threaded
  - L31: * when accessed from multiple threads. The Database object should be used by
  - L48: - Make logging compile-time optional
  - L66: - Make fsync optional via configuration
  - L151: Agent A should implement these items even though Agent C cannot create tests for them. The implementation should follow the patterns established in the existing codebase.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/legacy-tests/WEEK3_WEEK4_TEST_SUMMARY.md
- Potential ambiguity markers detected:
  - L100: The failed comprehensive tests identify areas for future enhancement rather than bugs in the current implementation. The system successfully compiles basic SQL to bytecode, which meets the Alpha 1.05 requirements.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_session_docs/PHASE_1_5_QUICK_START.md
- Potential ambiguity markers detected:
  - L49: **Optional**:
  - L255: - [ ] First successful compilation (may have warnings)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_session_docs/PHASE_1_5_README.md
- Potential ambiguity markers detected:
  - L387: # Count TID conversions (should be minimal, only at boundaries)
  - L390: # Check if any headers still have uint64_t tuple_id (should be 0)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/legacy/phase1_session_docs/PHASE_1_5_WORK_COMPLETED.md
- Potential ambiguity markers detected:
  - L248: | Compilation Fixes | ❌ 0% | TBD | 0h | 4-6h |
  - L545: - May miss edge cases
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_001_COMPLETE.md
- Potential ambiguity markers detected:
  - L44: - **Type-safe APIs** with std::optional
  - L269: - **Type safety:** std::optional for all nullable returns
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_1_COMPLETE.md
- Potential ambiguity markers detected:
  - L155: - `/docs/specifications/parser/v3/status/TODO.md` - Updated with Phase 1 completion
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_2_COMPLETE.md
- Potential ambiguity markers detected:
  - L147: ID domain_id;              // Optional: field uses a domain
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_4_COMPLETE.md
- Potential ambiguity markers detected:
  - L25: - Element type validation (reject UNKNOWN)
  - L70: 4. **Reject UNKNOWN element type** - Validation of element type
  - L178: - Element type cannot be UNKNOWN
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_002_PHASE_5_COMPLETE.md
- Potential ambiguity markers detected:
  - L26: - Reject UNKNOWN types
  - L55: 4. Reject UNKNOWN type
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_1_COMPLETE.md
- Potential ambiguity markers detected:
  - L215: The following are marked as TODO and will be implemented in later phases:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_3_COMPLETE.md
- Potential ambiguity markers detected:
  - L632: **Phase 5 (Optional):**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/ALPHA_003_GIN_PHASE_5_COMPLETE.md
- Potential ambiguity markers detected:
  - L180: **Current Implementation**:
  - L182: - Tree traversal optimization TODO
  - L200: **Current Implementation**:
  - L202: - Index traversal logic TODO
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/MGA_COMPLIANCE_COMPLETE.md
- Potential ambiguity markers detected:
  - L312: ### Enhancements (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE1_STRUCTURE_ANALYSIS_REPORT.md
- Potential ambiguity markers detected:
  - L239: - ⚠️ Root-level clutter (19 files, should be 5-8 core files)
  - L706: - 19 files at docs/ root (should be 5-8)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE1_UTF8_UTILS_COMPLETE.md
- Potential ambiguity markers detected:
  - L203: **Note**: Test execution pending build system availability. Code follows existing patterns and should compile/pass without issues.
  - L288: - Mitigation: Code follows existing patterns, should compile/pass
  - L293: - Impact: Unknown performance characteristics for large strings
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE2_CATALOG_STORAGE_EXPANSION_COMPLETE.md
- Potential ambiguity markers detected:
  - L95: **Rationale for Non-Changes**: These fields are not SQL identifiers subject to the 128-character standard. User/role names may need separate expansion in future phases if required.
  - L332: - Future: May expand in later phases if required
  - L350: - Future: May need TOAST support for large defaults
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE2_IMPLEMENTATION_STATUS_REPORT.md
- Potential ambiguity markers detected:
  - L780: **Action 3: Archive Scattered Index Specs (Optional)**
  - L871: **Strategy 3: REORGANIZE** (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE3_CATALOG_WRITE_LOGIC_FIXES_COMPLETE.md
- Potential ambiguity markers detected:
  - L490: - Future: May expand tablespace.h if needed (low priority)
  - L496: - Future: May need TOAST support for large defaults
  - L546: ### 4. Error Messages Should Be Specific
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE3_STORAGE_ENGINE_INTEGRATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L74: [TupleHeader (44 bytes)] [Null Bitmap (optional)] [Column Data]
  - L136: Per PHASE_3_REVISED_TASKS.md, the following manual tests should be performed:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE4_CATALOG_READ_SAFETY_COMPLETE.md
- Potential ambiguity markers detected:
  - L173: - Benefit: Protection against undefined behavior from corrupted data
  - L199: readIndexRecords, readTablespaceRecords). Protects against undefined
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE4_GARBAGE_COLLECTION_COMPLETE.md
- Potential ambiguity markers detected:
  - L152: TODO: Clear xmax (chunk still alive)
  - L163: - Collects TIDs for chunks with aborted xmax (TODO: clear xmax)
  - L167: - Line 1407-1409: TODO to implement xmax clearing for aborted deletes
  - L577: **Known Issues**: xmax clearing for aborted deletes not implemented (TODO), requires end-to-end testing.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE5_SQL_UTF8_TESTING_COMPLETE.md
- Potential ambiguity markers detected:
  - L408: - May need adjustment based on actual catalog introspection API
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE5_TESTING_VALIDATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L318: - detoastIfNeeded may be called in btree.cpp instead of separate index files
  - L446: **Current State**: ScratchBird may have fewer index types than PostgreSQL
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE6_DOCUMENTATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L320: ### Short-term (Optional Phase 7)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE6_DOCUMENTATION_OPTIMIZATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L228: - Phase 6: TBD - Documentation (this phase)
  - L367: ### Immediate (Optional)
  - L443: **Lesson**: API documentation should explicitly call out:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_2_TASK_9_3_GEOMETRY_CONSTRUCTORS_COMPLETE.md
- Potential ambiguity markers detected:
  - L196: - Each query should create one GEOSContext per thread
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_3_INTERVAL_TYPE_COMPLETE.md
- Potential ambiguity markers detected:
  - L36: - "1 month from March 31" → April 30 (not May 1)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_4_DECIMAL_ARITHMETIC_COMPLETE.md
- Potential ambiguity markers detected:
  - L239: - Current implementation is straightforward
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_5_JSONB_TYPE_COMPLETE.md
- Potential ambiguity markers detected:
  - L56: - Returns `std::optional` for safe access
  - L243: - Safe with `std::optional` returns
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_6_XML_TYPE_COMPLETE.md
- Potential ambiguity markers detected:
  - L68: - Access attributes: `node->getAttribute(key)` returns `std::optional<string>`
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/PHASE_7_VECTOR_TYPE_COMPLETE.md
- Potential ambiguity markers detected:
  - L81: - Type safety with `std::optional` returns
  - L364: - Returns `std::optional` for safety
  - L370: - Mathematically undefined, but practical choice
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/SESSION_SUMMARY_PHASE3_COMPLETE.md
- Potential ambiguity markers detected:
  - L94: - TODO: Verify index contains actual value (needs catalog integration)
  - L100: - TODO: Verify index updated with new value
  - L105: - TODO: Verify NO index maintenance performed
  - L110: - TODO: Verify detoasting happens once (cache hit)
  - L120: - TODO: Test actual TOAST pointer detoasting
  - L341: **Mitigation**: Transaction rollback should handle cleanup (needs verification).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/SQL_IDENTIFIER_UTF8_FIX_COMPLETE.md
- Potential ambiguity markers detected:
  - L60: // Result: Buffer overrun, undefined behavior
  - L78: -- Result: Accepted (should be rejected)
  - L110: **Impact**: Unknown if identifiers persist correctly
  - L166: - 2-byte UTF-8: 64 characters L (should be 128)
  - L167: - 3-byte UTF-8: 42 characters L (should be 128)
  - L168: - 4-byte UTF-8: 32 characters L (should be 128)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/SQL_SPECIFICATION_IMPLEMENTATION_REPORT.md
- Potential ambiguity markers detected:
  - L62: - `TIMESTAMP` - parser.cpp:1334 (with optional `WITH TIME ZONE`)
  - L566: - **View Security** - Phase 3.6 TODO
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TABLESPACE_IMPLEMENTATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L459: std::optional<TID> resolve(const TID& source_tid);  // Find target TID
  - L747: #### Catalog Garbage Collection (Optional)
  - L899: - [x] No undefined behavior (ASAN clean)
  - L934: **SHOULD HAVE** (26-34 hours):
  - L947: ### Optional Enhancements (Post-ALPHA)
  - L992: **Phase 7 (Advanced Features)** is optional for ALPHA and estimated at 50-66 hours for core features (statistics, backup/restore, compression, encryption).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_14_PHASE_1_COMPLETE.md
- Potential ambiguity markers detected:
  - L237: ✅ **Error Handling**: `std::optional` for parsing (no exceptions)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_14_PHASE_3_OPERATORS_FUNCTIONS_COMPLETE.md
- Potential ambiguity markers detected:
  - L81: 4. **Normalization** (optional): Divide by log(1 + doc_length)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_14_PHASE_5_SQL_INTEGRATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L113: Converts text to tsvector with optional configuration.
  - L170: Parses tsquery from string with optional configuration.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_15_RANGE_TYPES_COMPLETE.md
- Potential ambiguity markers detected:
  - L219: - Adjacent ranges (should not overlap)
  - L235: - Adjacent ranges (should not be strictly left)
  - L239: - Adjacent ranges (should not be strictly right)
  - L243: - Overlapping ranges (should not be adjacent)
  - L244: - Ranges with gaps (should not be adjacent)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_16_NETWORK_TYPES_COMPLETE.md
- Potential ambiguity markers detected:
  - L19: 1. **INET** - IPv4/IPv6 addresses with optional netmask
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_FOUNDATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L161: | IDENTIFIER | ✅ | ✅ | With optional table qualifier |
  - L307: - Unknown function errors
  - L523: - Performance optimization (may reveal design issues)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_2_1_COMPLETE.md
- Potential ambiguity markers detected:
  - L1: # Task 17 MGA Phase 2.1 Complete: Optional Debug Logging
  - L15: Phase 2.1 (Optional Debug Logging) is now complete. Comprehensive debug logging has been added to all index maintenance operations for monitoring and troubleshooting.
  - L218: - May indicate rollback or update operation
  - L330: ✅ Backward compatible (logging is optional)
  - L401: **Phase**: 2.1 - Optional Debug Logging
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_2_2_COMPLETE.md
- Potential ambiguity markers detected:
  - L206: std::cerr << "May indicate high transaction contention" << std::endl;
  - L238: → May indicate complex expressions or multiple expression indexes
  - L391: - Optional debug logging
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_2_3_COMPLETE.md
- Potential ambiguity markers detected:
  - L399: - Optional debug logging for index maintenance
  - L428: **Why**: Current implementation filters visibility at the heap level (correct but slower). Phase 3 optimizes by filtering at the index level.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_3_1_COMPLETE.md
- Potential ambiguity markers detected:
  - L70: new_node->btn_xmin = 0; // TODO: Integrate with transaction manager
  - L110: - Added TODO comment for markDeleted() implementation
  - L269: - Added TODO for Phase 3.2
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_3_2_COMPLETE.md
- Potential ambiguity markers detected:
  - L186: ### Current Usage (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_MGA_PHASE_4_COMPLETE.md
- Potential ambiguity markers detected:
  - L80: - Search should find entry (xmin visible)
  - L97: - New snapshot should not see entry (deleted before snapshot)
  - L154: - Range scan should return only 100 visible entries
  - L253: All 11 tests should pass:
  - L447: ### Phase 5: Advanced Testing (Optional, 40-60 hours)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_PHASE_6_COMPLETE.md
- Potential ambiguity markers detected:
  - L268: - TODO: Add support for all data types (DATE, TIME, TIMESTAMP, etc.)
  - L271: - TODO: Implement proper Expression::toString() for EXPLAIN output
  - L274: - TODO: Implement TOAST storage for expressions > TOAST_TUPLE_THRESHOLD
  - L277: - TODO: Integrate with transaction manager for rollback support
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_PHASE_7_COMPLETE.md
- Potential ambiguity markers detected:
  - L23: - Evaluates predicates to determine if row should be indexed
  - L120: - Handles partial column INSERTs (fills unspecified columns with NULL)
  - L314: - TODO: Add support for DATE, TIME, TIMESTAMP, etc.
  - L317: - TODO: Cache deserialized expressions per-transaction
  - L320: - TODO: Implement TOAST storage for expressions > threshold
  - L323: - TODO: Integrate with transaction manager for rollback support
  - L326: - TODO: Batch B-tree operations for bulk DML
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_17_PHASE_9_COMPLETE.md
- Potential ambiguity markers detected:
  - L297: - ⏳ May miss some valid cases (false negatives acceptable)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_9_2_RTREE_PLANNER_COMPLETE.md
- Potential ambiguity markers detected:
  - L218: Expected EXPLAIN output should show:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TASK_9_SPATIAL_INTEGRATION_COMPLETE.md
- Potential ambiguity markers detected:
  - L138: **TODO**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TOAST_MGA_COMPLIANCE_COMPLETE.md
- Potential ambiguity markers detected:
  - L202: - Optional: Implement xmax clearing optimization
  - L294: - If xmax aborted: TODO - Clear xmax (chunk still alive)
  - L426: **Current State**: ScratchBird may have fewer index types than PostgreSQL
  - L548: - TBD - Phase 6 Complete: Documentation & Optimization
  - L643: - Optional optimizations (xmax clearing, parallel GC, TOAST statistics)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Alpha_Phase_1_Archive/status_archive/phase_completions/TOAST_MGA_PHASE3_ANALYSIS_COMPLETE.md
- Potential ambiguity markers detected:
  - L49: **Principle**: Indexes should NEVER know about TOAST pointers.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/BACKUP_AND_RESTORE.md
- Potential ambiguity markers detected:
  - L123: ├── PITR Archive (optional, low priority)
  - L244: - May include dead tuples (pre-sweep)
  - L308: | Index (optional)          |
  - L453: ### 4.5. Page Index (Optional)
  - L490: | 80 | 64 | `uint8_t[64]` | **Digital Signature** | Ed25519 signature (optional) |
  - L822: │ 6. REBUILD INDEXES (optional)                            │
  - L1144: 3. **Old Versions** - Backup may include dead tuples (before sweep)
  - L1182: **Issue:** Backup may include dead tuples if sweep has not run.
  - L2389: # Upload to S3 (optional)
  - L2564: | **Continuous** | Transaction Log | 7 days | PITR (optional) |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L28: - Any feature labeled **optional**, **deferred**, or **future** in this document is **REJECTED in V3**
  - L42: except items explicitly labeled optional/deferred/future.
  - L470: -- Test 3: Invalid JSON (should fail)
  - L520: assert(jsonb == NULL);  // Should fail with duplicate keys
  - L783: // JSON accessors always return JSON (may contain scalar)
  - L1911: -- Test 6: Invalid (should error)
  - L1932: **Problem:** SQL:2016 didn't specify how NULL values should be treated in UNIQUE constraints, leading to inconsistent behavior:
  - L1954: -- Default behavior (NULLS DISTINCT if not specified)
  - L2151: -- Test 4: Default behavior (should be NULLS DISTINCT)
  - L2501: - APIs may evolve in SQL:2026
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/BETA_SQL_STANDARD_COMPLIANCE_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L22: This specification defines the **complete implementation** of all missing SQL standard features identified in the gap analysis. This includes CRITICAL (must-have), HIGH (should-have), MEDIUM (nice-to-have), and LOW priority (future consideration) features.
  - L24: **All features in this specification are targeted for Beta implementation**, though they may be delivered in multiple Beta releases (Beta 1.0, 1.1, 1.2).
  - L616: - Window functions (should fail)
  - L1208: float weight;                      // Pre-computed weight (optional)
  - L1472: - CHECK constraints (optional - some DBs don't support)
  - L1620: // Re-validate constraint (data may have changed since recording)
  - L1682: **Problem:** Deferred constraints may need to lock referenced rows to prevent TOCTOU (time-of-check-to-time-of-use) issues.
  - L2264: **Priority:** 🟢 VERY LOW (optional extension)
  - L2292: **Priority:** 🟢 VERY LOW (optional extension)
  - L2294: **Recommendation:** Defer to optional extension
  - L2389: **optional extension:**
  - L2390: - CHECK with Subqueries (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/README.md
- Potential ambiguity markers detected:
  - L31: **Scope Note:** WAL references in cluster specs describe an optional write-after log stream for replication/PITR; MGA does not use WAL for recovery.
  - L39: - **[SBCLUSTER-NORMATIVE-LANGUAGE.md](SBCLUSTER-NORMATIVE-LANGUAGE.md)** - Normative language definitions (MUST, SHOULD, MAY)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md
- Potential ambiguity markers detected:
  - L130: bytes32 capability_matrix_hash;      // Optional feature flags
  - L328: These operations MAY proceed:
  - L437: Only users with `CLUSTER_CONFIG_ADMIN` privilege may sign epochs.
  - L454: For high-security deployments, epochs MAY require multiple administrator signatures:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-02-MEMBERSHIP-AND-IDENTITY.md
- Potential ambiguity markers detected:
  - L585: ### 6.4 Gossip-Based Observation (Optional Enhancement)
  - L587: For large clusters (>10 nodes), full peer observation (N×N) becomes expensive. An optional **gossip protocol** can reduce overhead:
  - L735: - Private keys SHOULD be encrypted at rest (filesystem encryption)
  - L736: - Private keys MAY be stored in HSM for high-security deployments
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-03-CA-POLICY.md
- Potential ambiguity markers detected:
  - L64: timestamp_t valid_until;     // Optional expiry (for overlap)
  - L88: string[] permitted_dns;      // DNS constraints (optional)
  - L89: string[] permitted_ips;      // IP constraints (optional)
  - L311: 3. Test new node join (should use CA_B)
  - L378: For large clusters (>10 nodes), full N×N observation may be expensive. **Quorum-based observation** is acceptable:
  - L662: - Generate new CA immediately (may use temporary CA)
  - L676: - Accept that some nodes may be disabled
  - L791: - CA certificate signing SHOULD be manual approval process
  - L792: - CA private keys SHOULD use ceremony with multiple parties for access
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-04-SECURITY-BUNDLE.md
- Potential ambiguity markers detected:
  - L172: UUID masking_function_uuid;          // Optional: masking function to apply
  - L217: string default_expression;           // Optional DEFAULT value
  - L445: abort();  // This should NEVER happen (leader validates before appending)
  - L564: - Administrator is alerted (cluster may be in inconsistent state)
  - L799: // Component hash should NOT match
  - L855: // This should FAIL (bundles only accepted via Raft)
  - L878: EXPECT_LT(elapsed, 30s);  // Should converge in < 30 seconds
  - L907: **Step 3: Test draft locally (optional)**
  - L913: SELECT * FROM orders WHERE order_id = 12345;  -- Should only see own tenant's orders
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-05-SHARDING.md
- Potential ambiguity markers detected:
  - L420: **Consistency**: Each shard uses its own snapshot; results may not be globally consistent (eventual consistency).
  - L433: **optional extension: Online Resharding**
  - L653: // shard3 may or may not equal shard1 (depends on hash collision)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-06-DISTRIBUTED-QUERY.md
- Potential ambiguity markers detected:
  - L650: **Implication**: Results may reflect inconsistent states across shards.
  - L670: **Problem**: Client writes to Shard 1, then immediately reads from all shards. Shard 1's new data is visible, but other shards may have stale views.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-07-REPLICATION.md
- Potential ambiguity markers detected:
  - L31: **Scope Note:** Write-after log (WAL) references describe an optional write-after log stream for replication/PITR; MGA does not use write-after log (WAL) for recovery.
  - L57: 4. **Synchronous Mode Optional**: Quorum-based synchronous replication for critical data
  - L85: 4. Primary → Client (ACK returned, may not wait for replicas)
  - L542: // This node may have been demoted
  - L695: After a failover, the original primary may come back online:
  - L718: - If primary fails before replica applies, data may be lost
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-08-BACKUP-AND-RESTORE.md
- Potential ambiguity markers detected:
  - L19: This document specifies the backup and restore architecture for ScratchBird clusters. Backups are performed **per-shard** with optional **cluster-consistent backup sets** for point-in-time recovery. A critical principle: **restore does not restore trust** - cryptographic material (certificates, CA, keys) is never backed up or restored.
  - L31: **Scope Note:** WAL references describe an optional write-after log stream for replication/PITR; MGA does not use WAL for recovery.
  - L55: 3. **Cluster-Consistent Sets**: Optional coordinated snapshots across all shards
  - L818: "Backup contains trust material - this should never happen!");
  - L905: // Restore to T2 (should have 10000 rows)
  - L948: // UUIDs should be different
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-09-SCHEDULER.md
- Potential ambiguity markers detected:
  - L68: │  - Determines when jobs should run                          │
  - L201: bytes result_data;                   // Optional result data
  - L210: FAILED,             // Failed (may retry)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-10-OBSERVABILITY.md
- Potential ambiguity markers detected:
  - L834: "Cluster may be split");
  - L1102: - SQL queries may contain PII (e.g., `WHERE email = 'alice@example.com'`)
  - L1103: - Span attributes should NOT include raw query parameters
  - L1200: // Verification should fail
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-12-AUTOSCALING_AND_ELASTIC_LIFECYCLE.md
- Potential ambiguity markers detected:
  - L174: Policies may include time windows:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-IMPLEMENTATION-BOUNDARY.md
- Potential ambiguity markers detected:
  - L29: - MUST vs SHOULD vs MAY requirements per milestone
  - L101: | ○ **RECOMMENDED** | SHOULD be implemented | Conformant but degraded |
  - L102: | △ **OPTIONAL** | MAY be implemented | No impact on conformance |
  - L114: | **Push Compute to Data** | ○ | ✓ | ✓ | Alpha may do naive routing |
  - L115: | **Identical Security Config** | ○ | ✓ | ✓ | Alpha may use simplified security |
  - L117: | **Trust Boundary Enforcement** | ○ | ✓ | ✓ | Alpha may backup keys (dev/test) |
  - L118: | **Immutable Audit Chain** | — | ✓ | ✓ | Alpha may use simple logging |
  - L157: | **Ed25519 keypairs** | ○ | ✓ | ✓ | Alpha may use simpler auth |
  - L162: | **Graceful node join/leave** | ○ | ✓ | ✓ | Alpha may require restart |
  - L191: | **Intermediate CAs** | — | — | △ | GA: optional delegation |
  - L192: | **Certificate transparency logs** | — | — | △ | GA: optional (compliance) |
  - L205: - Optional intermediate CAs for delegation
  - L250: | **Automated shard rebalancing** | — | — | △ | GA: OPTIONAL |
  - L273: | **Multi-shard scatter-gather** | ○ | ✓ | ✓ | Alpha: may be limited |
  - L279: | **Query plan caching** | — | — | △ | GA: OPTIONAL |
  - L280: | **Adaptive query routing** | — | — | △ | GA: OPTIONAL |
  - L309: | **Synchronous replication** | — | — | △ | GA: OPTIONAL |
  - L310: | **Quorum reads** | — | — | △ | GA: OPTIONAL |
  - L322: - Optional synchronous replication (strong consistency)
  - L339: | **Cross-region backup** | — | — | △ | GA: OPTIONAL |
  - ... 7 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-NORMATIVE-LANGUAGE.md
- Potential ambiguity markers detected:
  - L28: The key words **MUST**, **MUST NOT**, **REQUIRED**, **SHALL**, **SHALL NOT**, **SHOULD**, **SHOULD NOT**, **RECOMMENDED**, **MAY**, and **OPTIONAL** in the cluster specifications are to be interpreted as described in RFC 2119.
  - L46: ✗ INCORRECT: "Nodes should validate CCE signatures." (too weak for security)
  - L63: ✗ INCORRECT: "Backup procedures should not include private keys." (too weak)
  - L70: #### SHOULD / RECOMMENDED
  - L72: **Definition**: This word, or the adjective "RECOMMENDED", mean that there may exist valid reasons in particular circumstances to ignore a particular item, but the full implications must be understood and carefully weighed before choosing a different course.
  - L74: **Implication**: Implementations MAY deviate from SHOULD/RECOMMENDED clauses, but:
  - L77: 3. Deviation SHOULD be justified in implementation notes
  - L80: - Performance optimizations (e.g., "Query coordinators SHOULD cache shard routing tables")
  - L81: - Operational best practices (e.g., "Clusters SHOULD use at least 3 nodes for HA")
  - L82: - Non-critical features (e.g., "Metrics SHOULD be exported in Prometheus format")
  - L86: ✓ CORRECT: "Replication lag SHOULD be monitored via scratchbird_replication_lag_seconds metric."
  - L87: ✓ CORRECT: "Nodes SHOULD run health checks every 15 seconds." (can be tuned)
  - L90: #### SHOULD NOT / NOT RECOMMENDED
  - L92: **Definition**: This phrase, or the phrase "NOT RECOMMENDED", mean that there may exist valid reasons in particular circumstances when the particular behavior is acceptable or even useful, but the full implications should be understood and the case carefully weighed before implementing any behavior described with this label.
  - L94: **Implication**: Same as SHOULD (strong recommendation against, but not forbidden).
  - L97: - Discouraged patterns (e.g., "Administrators SHOULD NOT manually edit epoch records")
  - L98: - Performance anti-patterns (e.g., "Queries SHOULD NOT use SELECT * on sharded tables")
  - L102: ✓ CORRECT: "Applications SHOULD NOT issue queries that require full table scans across all shards."
  - L107: ### 2.3 Optional Features
  - L109: #### MAY / OPTIONAL
  - ... 53 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-SUMMARY.md
- Potential ambiguity markers detected:
  - L29: **Scope Note:** WAL references describe an optional write-after log stream for replication/PITR; MGA does not use WAL for recovery.
  - L243: - **Intermediate CAs**: Optional delegation (not required for alpha/beta)
  - L249: 2. **User Certificates**: For client authentication (optional)
  - L445: - **Eventual consistency**: Read from any replica (may be stale)
  - L716: ### Phase 4: Production Hardening (optional extension)
  - L774: ### Considered but Deferred (optional extension)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-THREAT-MODEL.md
- Potential ambiguity markers detected:
  - L127: - Peer observation may detect anomalies
  - L289: | Prepared statements (application-level) | N/A (app responsibility) | Applications SHOULD use prepared statements |
  - L309: | Least privilege principle | SBCLUSTER-04 (Security Bundle) | Roles SHOULD grant minimal privileges |
  - L331: **Mitigation Enhancement**: CA private key SHOULD be stored in HSM (Hardware Security Module).
  - L343: | Encryption keys NOT stored on disk | SBCLUSTER-04 (Key Management) | Keys SHOULD be retrieved from KMS/HSM |
  - L399: | Off-cluster audit archival | SBCLUSTER-10 (Audit) | Audit events MAY be exported to external SIEM |
  - L402: **Residual Risk**: **LOW** (tampering detected, but insider may still delete if quorum compromised)
  - L489: - **Mitigation**: CA key SHOULD be stored in HSM (SBCLUSTER-03)
  - L532: **If any assumption is violated**, the threat model MAY NOT hold, and additional mitigations MUST be applied.
  - L585: **Recommended tests (SHOULD)**:
  - L601: **Recommended chaos scenarios (SHOULD)**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Cluster Specification Work/sbsec_handoff_summary.md
- Potential ambiguity markers detected:
  - L52: - May be sandboxed or externally provided
  - L99: 9. Run optional security hooks (BeforeSelect / BeforeInsert)
  - L122: - May re-check authorization or policy epochs
  - L123: - May abort execution before row materialization
  - L130: - MAY modify results returned to the client
  - L131: - MAY fabricate or redirect data
  - L167: - MEK is OPTIONAL for Security Levels 0–5
  - L177: - Cluster operations may require quorum approval
  - L239: - Level 6 may cause irrecoverable loss
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/DATABASE_REGISTRY_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L65: database_alias      TEXT,                       -- Short alias (optional)
  - L157: expires_at          TIMESTAMP,                  -- Optional expiration
  - L344: --   - options: JSON with optional parameters
  - L460: IN session_stats JSON  -- Optional query stats from session
  - L513: std::optional<std::string> page_size;
  - L514: std::optional<std::string> charset;
  - L515: std::optional<std::string> security_model;
  - L516: std::optional<std::string> description;
  - L563: std::optional<DatabaseInfo> getDatabaseByName(
  - L566: std::optional<DatabaseInfo> getDatabaseById(
  - L805: - Registry should be backed up along with databases
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md
- Potential ambiguity markers detected:
  - L81: │   GC HANDLED BY SERVER (or optional background thread)                     │
  - L238: #include <optional>
  - L251: std::optional<int64_t> buffer_pool_size;
  - L252: std::optional<int> max_connections;
  - L304: std::optional<DatabaseInfo> getDatabaseById(const core::UUID& db_id);
  - L305: std::optional<DatabaseInfo> getDatabaseByName(const std::string& name);
  - L306: std::optional<DatabaseInfo> getDatabaseByAlias(const std::string& alias);
  - L527: std::optional<DatabaseInfo> DatabaseRegistry::getDatabaseByName(
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/DDL_ALTER.md
- Potential ambiguity markers detected:
  - L257: - Unknown object type: `ERR_DDL_UNSUPPORTED_OBJECT`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/DDL_CREATE.md
- Potential ambiguity markers detected:
  - L46: Defines a new schema path and optional authorization.
  - L144: - Unknown object type: `ERR_DDL_UNSUPPORTED_OBJECT`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md
- Potential ambiguity markers detected:
  - L68: - Unknown object type: `ERR_DDL_UNSUPPORTED_OBJECT`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/DELETE.md
- Potential ambiguity markers detected:
  - L14: 2. Parse target table reference and optional alias.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/DRIVER_CONFORMANCE_TEST_HARNESS.md
- Potential ambiguity markers detected:
  - L52: - `expect_rows`: optional row count assertion
  - L56: - `requires`: optional list of env-gated requirements
  - L77: Adapters may also check server-advertised capabilities to ensure a feature is
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/DRIVER_STREAMING_AND_PAGING.md
- Potential ambiguity markers detected:
  - L28: - Drivers should support repeated EXECUTE calls to fetch subsequent pages.
  - L29: - Where possible, drivers should expose a `fetch_size` (rows per page) option.
  - L33: - Large values may be returned as streamed columns (length = -2 in DATA_ROW).
  - L48: - Optional: expose fetch size via driver-specific options.
  - L63: - ResultStream#each should yield rows incrementally.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md
- Potential ambiguity markers detected:
  - L53: If any step fails, no physical write may occur.
  - L63: - GC may remove old versions only when no active transaction can see them.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L21: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L22: Any WAL references in this document describe an optional extension stream for
  - L124: **Current Implementation:**
  - L217: - ⚠️ Needs implementation - should support RDB$GET_CONTEXT/RDB$SET_CONTEXT as regular functions
  - L231: - These should be treated as special column references, not functions
  - L597: // Optional ON COMMIT clause for GTT
  - L605: matchKeyword(TokenType::KW_ROWS);  // Optional ROWS keyword
  - L624: NONE = 0,           // Not specified
  - L699: - Do NOT write to the optional write-after log stream
  - L707: SELECT * FROM session_temp;  -- Should return 1 row
  - L709: SELECT * FROM session_temp;  -- Should fail (table doesn't exist)
  - L715: SELECT * FROM tx_temp;  -- Should return 1 row
  - L717: SELECT * FROM tx_temp;  -- Should return 0 rows (truncated)
  - L724: SELECT * FROM tx_temp2;  -- Should return 1 row (preserved)
  - L730: SELECT * FROM my_temp;  -- Should fail (not visible to other sessions)
  - L737: SELECT * FROM drop_on_commit;  -- Should fail (table dropped)
  - L773: #### What UNLOGGED Tables Should Do
  - L775: **Purpose:** Optimize performance by skipping optional write-after log writes
  - L777: **MGA Note:** ScratchBird does not use WAL for recovery, so UNLOGGED tables are effectively identical to regular tables today. If a write-after log (WAL, optional extension) is introduced later (replication/PITR), UNLOGGED tables can bypass that stream.
  - L780: - NOT written to write-after log (WAL, optional)
  - ... 23 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/IMPLEMENTATION_STANDARDS.md
- Potential ambiguity markers detected:
  - L238: 6. **TODO Verification**
  - L241: grep -r "TODO" src/ include/ | grep -i "<feature>" || echo "No TODOs found"
  - L265: ### TODO Verification
  - L343: - TODO verification
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/INSERT.md
- Potential ambiguity markers detected:
  - L14: 2. Parse target table reference and optional column list.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L393: ; Client certificate verification: none, optional, require
  - L481: result.addWarning("network.native_port", "Port may be in use");
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/JOINS.md
- Potential ambiguity markers detected:
  - L39: - `alias`: optional alias
  - L40: - `alias_columns`: optional list of column aliases
  - L41: - `subquery`: optional SELECT AST (for SUBQUERY/LATERAL_SUBQUERY)
  - L42: - `func_args`: optional list of expressions (for FUNCTION)
  - L50: - `using_columns`: optional list of identifiers
  - L51: - `on_expr`: optional expression
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/MEMORY_MANAGEMENT.md
- Potential ambiguity markers detected:
  - L21: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in V3. WAL is disabled for recovery; if enabled, it exists only as an optional replication/PITR stream.
  - L22: Any WAL references in this document describe an optional replication/PITR stream for
  - L24: **Table Footnote:** In comparison tables below, ScratchBird WAL references are optional extension (replication/PITR).
  - L104: │   - jemalloc (optional)                              │
  - L193: **ScratchBird uses Firebird MGA, not PostgreSQL write-after log (WAL, optional extension):**
  - L195: - **No write-after log (WAL, optional extension) Buffers** - MGA doesn't require write-after log (WAL, optional extension) for recovery
  - L443: - Multiple threads may allocate concurrently
  - L946: ### 6.2. jemalloc (Optional)
  - L1552: // Destroy context (should free all allocations)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/MGA_RULES.md
- Potential ambiguity markers detected:
  - L383: - 100 back versions (may be delta-compressed)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/MYSQL_PARSER_IMPLEMENTATION_GAPS.md
- Potential ambiguity markers detected:
  - L26: - Any item marked **UNKNOWN** is **out-of-scope** until explicitly defined.
  - L68: - Catalog persists these options as table metadata; executor ignores unknown options.
  - L246: // This should be done in parseColumnDef() after type
  - L284: // Optional modifiers
  - L431: // Optional column list
  - L461: **Status:** ⚠️ **UNKNOWN - Needs Verification**
  - L462: **Impact:** Table metadata may not be stored
  - L534: **Current Implementation:** Maps to ON CONFLICT DO UPDATE (PostgreSQL semantics)
  - L615: ### Phase 2: Important Features (optional extension) - 3-4 days
  - L722: | ENGINE | ✅ | ❓ | ❓ | **UNKNOWN** | **MEDIUM** |
  - L723: | CHARSET/COLLATE | ✅ | ❓ | ❓ | **UNKNOWN** | **MEDIUM** |
  - L724: | COMMENT | ✅ | ❓ | ❓ | **UNKNOWN** | **MEDIUM** |
  - L1018: ### Phase 2: optional extension (HIGH)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/OFFICIAL_ROADMAP.md
- Potential ambiguity markers detected:
  - L130: 6. **Optional Engine Enhancements**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/PARSER_REMAPPING_AND_IMPLEMENTATION_STRATEGY.md
- Potential ambiguity markers detected:
  - L21: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L22: Any WAL references in this document describe an optional extension stream for
  - L140: // Optional: USING {BTREE | HASH}
  - L277: **Decision:** ✅ **ALREADY REMAPPED** - Keep current implementation
  - L283: - Current implementation works
  - L285: **Current Implementation:**
  - L348: **Priority:** **MEDIUM** (optional extension)
  - L386: **Priority:** **MEDIUM** (optional extension)
  - L401: **Current Implementation:**
  - L420: These features should be explicitly rejected with clear error messages rather than silently failing.
  - L425: **Decision for Beta:** 🟡 **OPTIONAL** (only if write-after log is introduced)
  - L429: - MGA has no write-after log (WAL, optional extension), so UNLOGGED semantics are effectively identical to regular tables
  - L430: - If a write-after log (WAL, optional extension) is introduced later (for replication/PITR), UNLOGGED can bypass it
  - L454: - Can be added optional extension if needed
  - L457: **Current Implementation:**
  - L466: **Priority:** **optional extension** (if requested)
  - L482: **Current Implementation:**
  - L490: **Priority:** **optional extension** (optimization)
  - L544: **Priority:** **LOW** (optional warning)
  - L560: - Some are MySQL-specific and should be ignored (ENGINE, ROW_FORMAT)
  - ... 8 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md
- Potential ambiguity markers detected:
  - L77: - `USING` expression may reference old column; must be emitted before type update.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/PERFORMANCE_BENCHMARKS.md
- Potential ambiguity markers detected:
  - L77: - **Multi-Version In-Page** - Page updates may be larger than PostgreSQL
  - L91: Note: optional write-after log (WAL) for replication/PITR may reintroduce write-after log (WAL)-style overhead.
  - L185: Storage: 1 TB NVMe SSD (RAID 10 optional)
  - L668: # Write-after log (WAL, optional; not used for recovery in MGA)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md
- Potential ambiguity markers detected:
  - L25: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L26: Any WAL references in this document describe an optional extension stream for
  - L39: - Any item marked **UNKNOWN** is **out-of-scope** until explicitly defined.
  - L270: - Table is not written to the optional write-after log (WAL, optional extension)
  - L271: - Faster writes (no write-after log (WAL, optional extension) overhead)
  - L276: - MGA does not use write-after log (WAL, optional extension) for recovery, so UNLOGGED tables are effectively the same as regular tables today.
  - L277: - If an optional write-after log (WAL, optional extension) is introduced later (replication/PITR), UNLOGGED can bypass that stream.
  - L279: **Result:** `CREATE UNLOGGED TABLE test (id INT)` creates a normal table. Under MGA, this is acceptable; any difference only appears if a write-after log (WAL, optional extension) is added later.
  - L288: 4. Skip write-after log (WAL, optional extension) if introduced
  - L289: 5. Truncate on crash recovery only if a write-after log (WAL, optional extension) durability path is introduced
  - L292: **Priority:** **optional extension** (optimization feature)
  - L350: **Priority:** **optional extension**
  - L393: **Priority:** **optional extension**
  - L436: **Priority:** **optional extension**
  - L487: - Risk: May break other parsers
  - L547: **Status:** ⚠️ **PARSED - Implementation Unknown**
  - L591: **Priority:** **optional extension**
  - L659: ### Phase 2: optional extension Features - 5-7 days
  - L688: - Implement write-after log (WAL, optional extension) bypass (optional)
  - L689: - Implement crash recovery truncation if write-after log (WAL, optional extension) durability is introduced
  - ... 7 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/PROJECT_CONTEXT.md
- Potential ambiguity markers detected:
  - L38: - **Optional libraries:** Keep minimal deps; OpenSSL is build-time; others (GEOS/PROJ/libxml2/LZ4) are optional. Runtime-load idea captured separately.
  - L43: - **Triggers:** Before/after for DB/table (SELECT support TBD); ordered by smallint; ensure runtime hooks.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/PSQL_RUNTIME_V3.md
- Potential ambiguity markers detected:
  - L103: payload specifies UNKNOWN handling).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/README.md
- Potential ambiguity markers detected:
  - L8: implementation-first view. The intent is to document *how the parser is currently implemented*
  - L45: ## V3 Documents (Implementation-First)
  - L48: current implementation locations (path/file/line). All command docs are written to
  - L119: - All file/line references are to the current implementation and must be revalidated
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md
- Potential ambiguity markers detected:
  - L14: - optional debug and integrity sections without breaking compatibility.
  - L41: timestamp_utc:u64   // unix epoch seconds, 0 if unknown
  - L42: module_id[16]       // UUID v7 (binary), 0 if unknown
  - L62: `section_flags` are section-specific. Unknown flags must be ignored for
  - L72: | 0x0005 | DEPENDENCIES | optional |
  - L73: | 0x0006 | DEBUG_INFO | optional |
  - L74: | 0x0007 | INTEGRITY | optional |
  - L86: build_id:string        // optional build label
  - L87: source_hash:bytes      // optional hash of source (0 length = absent)
  - L136: ### DEPENDENCIES (optional)
  - L150: ### DEBUG_INFO (optional)
  - L165: ### INTEGRITY (optional)
  - L166: Provides container integrity and optional signatures.
  - L187: - Unknown sections must be ignored but preserved if rewriting.
  - L190: Loaders must ignore unknown section IDs and unknown section flags. New optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md
- Potential ambiguity markers detected:
  - L11: - Explicit rules for when pooling is required, optional, or forbidden.
  - L34: ### optional Symbol Pooling
  - L35: These MAY be pooled to reduce size, but are not required:
  - L68: ### optional Constant Pooling
  - L69: The following MAY be pooled:
  - L104: - LITERAL opcodes may inline payloads only when pooling is explicitly forbidden
  - L119: 5. optional: EXCEPTION_TABLE bytes if present.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md
- Potential ambiguity markers detected:
  - L213: [path:schema_path]        // table or schema-qualified path (may be empty)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md
- Potential ambiguity markers detected:
  - L252: | SBLR3_ON_CONFLICT (0x061F) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
  - L253: | SBLR3_ON_CONFLICT_COLUMN (0x0621) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
  - L254: | SBLR3_ON_CONFLICT_CONSTRAINT (0x0623) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
  - L255: | SBLR3_ON_CONFLICT_DO_NOTHING (0x0625) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
  - L256: | SBLR3_ON_CONFLICT_DO_UPDATE (0x0627) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
  - L257: | SBLR3_ON_CONFLICT_WHERE (0x0629) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
  - L317: | SBLR3_EXPR_ILIKE (0x070A) | pop(left,right) | push(result) | Pattern match using LIKE semantics, optional ESCAPE, and collation rules; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
  - L319: | SBLR3_EXPR_LIKE (0x070C) | pop(left,right) | push(result) | Pattern match using LIKE semantics, optional ESCAPE, and collation rules; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
  - L330: | SBLR3_ILIKE_ESCAPE (0x0717) | pop(left,right) | push(result) | Pattern match using LIKE semantics, optional ESCAPE, and collation rules; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
  - L331: | SBLR3_LIKE_ESCAPE (0x0718) | pop(left,right) | push(result) | Pattern match using LIKE semantics, optional ESCAPE, and collation rules; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
  - L524: | SBLR3_TYPE_UNKNOWN (0x0B00) | - | push(type_descriptor) | Push type descriptor for type unknown onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
  - L755: | SBLR3_STRING_TO_ARRAY (0x0F49) | pop(args...) | push(result) | Split string into array using delimiter and optional null string; return array of text. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md
- Potential ambiguity markers detected:
  - L24: - No unknown opcode values are permitted.
  - L36: | `V3E-0002` | STRUCT | Unknown opcode (not present in `SBLR_V3_OPCODE_SPEC.md`). |
  - L40: | `V3E-0012` | TYPE | TYPE_SPEC missing required flags for provided optional fields. |
  - L96: 3. No opcodes may follow `SBLR3_END`.
  - L185: - `dimensions` MUST equal `len(dim_lengths)` and may be 0 for empty arrays.
  - L199: - `blob_length` may be zero; if non-zero, it must match the declared blob length in the catalog (if available).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SCRATCHBIRD_ARCHITECTURE_OVERVIEW.md
- Potential ambiguity markers detected:
  - L119: │  │  │  │  (optional)  │  │              │  │                                             │  │   │
  - L267: │   Optional               │     │  • Execute startup SQL scripts                          │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SCRATCHBIRD_CONNECTION_RECOVERY_MODEL.md
- Potential ambiguity markers detected:
  - L140: - "Last query: FAILED/COMPLETED/UNKNOWN"
  - L176: | Query status unknown | Keep tx active | "Reconnected, triage required" |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L107: - Application responsible for GC (optional thread)
  - L198: nullptr                     // Options (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SCRATCHBIRD_SECURITY_AND_ACCESS_MODEL.md
- Potential ambiguity markers detected:
  - L52: │ Database List    │ (or empty if none) │ (may be empty)     │ cluster config │
  - L62: │                  │ (full control)     │ server may limit)  │ cluster-wide   │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md
- Potential ambiguity markers detected:
  - L99: - Cluster Manager (optional)
  - L202: **Why this design:** Long-running analytical work should not be lost due to transient connectivity; the engine can finish work and resume delivery once the session is restored.
  - L241: - Max reconnect attempts are configurable and should alert monitoring.
  - L251: - Registry access is controlled by `sys.sec` roles; only admins may write.
  - L259: - Unknown keys MUST be rejected with `ERR_INVALID_CONFIG_KEY`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SELECT_AND_QUERY.md
- Potential ambiguity markers detected:
  - L14: 2. Parse optional WITH clause (CTEs) in order.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md
- Potential ambiguity markers detected:
  - L178: The database registry maintains a catalog of all databases known to the server instance. Each port listener may have its own registry (enabling MSSQL-style "named instances"). When a client connects, they receive a list of databases they can access from the registry.
  - L442: - Optional client certificate verification
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/SESSION_AND_UTILITY.md
- Potential ambiguity markers detected:
  - L10: It is implementation-first and aligned with current parser code.
  - L45: - ON CONFLICT (COMMIT/ROLLBACK/ERROR/KEEP, optional error code)
  - L52: ### Parsing Algorithm (Current Implementation)
  - L54: 1. Parse optional scope: `SESSION` or `LOCAL`.
  - L111: - Parsed as `SHOW COLUMNS` for a table with optional column/like pattern.
  - L122: - optional `VERBOSE`.
  - L124: - optional column (only one allowed) using either `(col)` or `COLUMN col`.
  - L125: - optional `SAMPLE <float|int>` (only one allowed).
  - L136: - optional clauses (any order): `USER`, `PASSWORD`, `ROLE`, `CHARSET`/`CHARACTER SET`.
  - L239: - `42704` undefined_object (unknown SHOW target)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/00_SECURITY_SPEC_INDEX.md
- Potential ambiguity markers detected:
  - L19: - Deprecated mechanisms may only be enabled via explicit configuration **and** must emit warnings.
  - L21: - No new feature may depend on deprecated mechanisms.
  - L274: - **○** = Optional at this security level
  - L281: - Box with dashed lines: Optional component
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/01_SECURITY_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L40: The keywords MUST, MUST NOT, REQUIRED, SHALL, SHALL NOT, SHOULD, SHOULD NOT, RECOMMENDED, MAY, and OPTIONAL in this document are to be interpreted as described in RFC 2119.
  - L69: The Engine Core is the sole authority for all security decisions. No other component (parser, network listener, client, plugin) may bypass, override, or influence security enforcement.
  - L342: The Engine MAY send the following commands to the Network Listener:
  - L642: | Locks              | Sessions may block on locks but cannot see each other's lock state |
  - L688: c. May spawn replacement parser for pool
  - L693: Administrators may terminate sessions:
  - L709: Sessions may be terminated for security reasons:
  - L744: │  • Active role (from session, may be NONE)                       │
  - L827: The parser may carry query representation and produce SBLR, but it is not trusted to enforce authorization or security policies. **Row-Level and Column-Level Security (RLS/CLS) MUST be determined from the engine catalog and applied by Engine Core during plan construction/approval (engine-side rewrite or engine-validated policy gates).**
  - L905: Compiled SBLR may be cached for performance. Cache security:
  - L922: ### 8.2 Current Implementation (Alpha)
  - L1022: | Optional           | ●   |     |     |     |     |     |     |
  - L1041: ● = Required | ○ = Optional | (blank) = Not applicable
  - L1067: No parser, listener, plugin, or external component may bypass Engine enforcement.
  - L1071: > Sessions are completely isolated. No session may access another session's state, memory, or transaction context.
  - L1145: | Administrative session termination timeout | To be determined | Policy configuration    |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/02_IDENTITY_AUTHENTICATION.md
- Potential ambiguity markers detected:
  - L51: ● = Required | ○ = Optional | (blank) = Not applicable
  - L87: │  │  • Usage limits (optional)                              │    │
  - L88: │  │  • Allowed roles subset (optional)                      │    │
  - L165: - May be referenced in audit logs indefinitely after user deletion
  - L170: - Usernames may be changed without affecting permissions
  - L203: not_before          TIMESTAMP,              -- Optional: validity window start
  - L244: │       - Optional restrictions (roles, addresses)                 │
  - L247: │     • Session may be created using AuthKey                       │
  - L258: │     • No new sessions may use this AuthKey                       │
  - L321: A single AuthKey may be used to create multiple sessions (subject to max_uses):
  - L324: - Sessions may have different active roles (within AuthKey's allowed_roles)
  - L667: # Common password check (optional)
  - L802: | Registration | Only CLUSTER_ADMIN may register providers |
  - L804: | Signature | Optional cryptographic signature verification |
  - L1210: - Client should retry or fail
  - L1241: | CAPTCHA | Optional for repeated failures |
  - L1249: | Session binding | Optional IP/fingerprint binding |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/03_AUTHORIZATION_MODEL.md
- Potential ambiguity markers detected:
  - L62: ● = Required | ○ = Optional | (blank) = Not applicable
  - L263: - **Home schema**: Optional default schema used when the role is active
  - L330: - **Home schema**: Optional default schema used when a user has no role/user default
  - L609: │     • May expand wildcards (ALL TABLES IN SCHEMA)                │
  - L754: │  Note: RLS/CLS policy gates are determined and applied by the Engine during plan construction/approval (engine-side rewrite). The parser may carry query representation, but it is not trusted to enforce policies.     │
  - L761: Certain users may bypass RLS:
  - L930: Non-strict deployments (security levels 0–2) MAY offer limited, read-only catalog access for development convenience,
  - L1002: Users may need to access objects across dialects:
  - L1090: An object (table, view, function, etc.) MAY be marked `REVOCATION_SENSITIVE`, or a policy MAY require immediate enforcement.
  - L1100: The hook is executed by Engine Core (not the parser) and MAY consult:
  - L1119: - Levels 0–4: `BeforeSelect` is optional and typically disabled.
  - L1120: - Level 5: `BeforeSelect` MAY be required for objects flagged `REVOCATION_SENSITIVE`.
  - L1121: - Level 6: `BeforeSelect` SHOULD run per batch for revocation-sensitive objects, and MAY be combined with NPB/MEK policies.
  - L1163: │  • Session may be terminated                                     │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/04.01_KEY_LIFECYCLE_STATE_MACHINES.md
- Potential ambiguity markers detected:
  - L32: - `ROTATING`: rotation in progress; key may still be required for reads/decrypt.
  - L69: If a key is `ROTATING` but not selected as active, the engine MUST treat it as “prepared but not activated” and may:
  - L102: - Levels 0–2: rotation MAY be manual; audit optional.
  - L103: - Levels 3–4: rotation SHOULD be supported; audit required per SBSEC-09.
  - L105: - Level 6: quorum rules MAY be required for CMK/MEK changes; see SBSEC-06.01/06.02.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/04.02_KEY_MATERIAL_HANDLING_REQUIREMENTS.md
- Potential ambiguity markers detected:
  - L36: - optional `retire_at`, `destroy_at`.
  - L47: Key material MUST NEVER appear in logs. Keys may be referenced by:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/04.03_NONCE_IV_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L7: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L8: Any WAL IV rules in this document apply only to an optional extension WAL stream
  - L47: ## 4. Write-after log (WAL, optional extension) / Log IV
  - L55: - `wal_incarnation_32` MUST increment whenever the write-after log (WAL, optional extension) stream is reset/rebased or when the write-after log (WAL, optional extension) key_version changes.
  - L56: - `lsn_64` MUST be unique within a write-after log (WAL, optional extension) incarnation.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/04_ENCRYPTION_KEY_MANAGEMENT.md
- Potential ambiguity markers detected:
  - L13: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L14: Any WAL encryption in this document applies only to an optional extension WAL
  - L30: - Write-after log (WAL, optional extension) and transaction log encryption
  - L51: | Write-after log (WAL, optional extension) encryption | | | ● | ● | ● | ● | ● |
  - L58: ● = Required | ○ = Optional | (blank) = Not applicable
  - L66: **MEK scope by security level:** The **Memory Encryption Key (MEK)** is an optional runtime key intended for
  - L138: | LEK | Transaction Log | DBK | Write-after log (WAL, optional extension) entries | Until rotation |
  - L569: MUST be prevented by construction. This section defines **normative** IV generation rules for page encryption, write-after log (WAL, optional extension)/log
  - L591: - Replaying write-after log (WAL, optional extension) MUST NOT reuse an `(IV, key_version)` pair for any page.
  - L593: #### 6.4.2 Write-after log (WAL, optional extension) / Log IV
  - L595: Write-after log (WAL, optional extension)/log records MUST use an IV derived from a value that never repeats for the same key:
  - L598: Where `wal_incarnation_32` increments whenever the write-after log (WAL, optional extension) stream is logically reset/rebased or when the encryption key_version changes.
  - L621: ## 7. Write-after log (WAL, optional extension) and Log Encryption
  - L625: The LEK encrypts write-after log (WAL) entries (optional):
  - L629: """Encrypt a write-after log (WAL, optional extension) record."""
  - L653: ### 7.2 Write-after log (WAL, optional extension) Record Format
  - L657: │              ENCRYPTED Write-after log (WAL, optional extension) RECORD              │
  - L679: ### 7.3 Write-after log (WAL, optional extension) Key Rotation
  - L681: LEK rotation must be coordinated with write-after log (WAL, optional extension) archival:
  - L684: 2. Mark rotation point in write-after log (WAL, optional extension)
  - ... 5 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/05.A_IPC_WIRE_FORMAT_AND_EXAMPLES.md
- Potential ambiguity markers detected:
  - L27: u32  aad_len;        // length of AAD bytes that follow (optional; usually 0, engine-defined)
  - L51: - Frames with `seq > expected_seq` MAY be rejected or buffered; the default is reject (simpler and safer).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/05_IPC_SECURITY.md
- Potential ambiguity markers detected:
  - L61: ● = Required | ○ = Optional | (blank) = Not applicable
  - L775: | 1002 | INVALID_TOKEN | Unknown or expired token |
  - L783: | 2001 | INTERNAL_ERROR | Unspecified internal error |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/06.01_QUORUM_PROPOSAL_CANONICALIZATION_HASHING.md
- Potential ambiguity markers detected:
  - L27: - `policy_epoch` (optional but recommended)
  - L50: - 0x09 policy_epoch (u64) optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/06.02_QUORUM_EVIDENCE_AND_AUDIT_COUPLING.md
- Potential ambiguity markers detected:
  - L44: At higher security levels, the audit event SHOULD include a signature over the event hash.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/06_CLUSTER_SECURITY.md
- Potential ambiguity markers detected:
  - L53: ● = Required | ○ = Optional | (blank) = Not applicable
  - L78: │  • May also be a Data Node                                      │
  - L212: │  • Reads may go to any Security Node (with replication lag)     │
  - L641: │  Optional Hard Fencing (STONITH):                               │
  - L815: │  • Unknown users cannot authenticate                            │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/07_NETWORK_PRESENCE_BINDING.md
- Potential ambiguity markers detected:
  - L49: - **OPTIONAL** at Security Level 5
  - L597: Shares should be periodically refreshed without changing the secret:
  - L711: • Admin should replace failed node
  - L712: • Optional: refresh shares to exclude Node 3's share
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/08.01_AUDIT_EVENT_CANONICALIZATION.md
- Potential ambiguity markers detected:
  - L27: - 0x04 principal_id (UUID) optional for system events
  - L28: - 0x05 session_id (UUID) optional
  - L29: - 0x06 database_id (UUID) optional
  - L30: - 0x07 object_ids (repeated UUID) optional
  - L31: - 0x08 decision (u16) optional
  - L32: - 0x09 details (nested TLV) optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/08.02_AUDIT_CHAIN_VERIFICATION_CHECKPOINTS.md
- Potential ambiguity markers detected:
  - L10: Defines verification for the tamper-evident audit chain, checkpointing, and optional signatures.
  - L25: - optional signature by node/cluster key
  - L28: Checkpoint interval is configuration; Level 4+ SHOULD use periodic checkpoints.
  - L38: ## 5. Optional Signatures
  - L40: At higher levels, events or checkpoints MAY include signatures. If used, the signature MUST cover:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/08_AUDIT_COMPLIANCE.md
- Potential ambiguity markers detected:
  - L52: ● = Required | ○ = Optional | (blank) = Not applicable
  - L971: - **SBSEC-08.02 Audit Chain Verification, Checkpoints, and Optional Signatures**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/09_SECURITY_LEVELS.md
- Potential ambiguity markers detected:
  - L13: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L14: Any WAL encryption requirements in this document apply only to an optional
  - L15: optional extension WAL stream for replication/PITR.
  - L23: This document defines the seven security levels (0-6) supported by ScratchBird. Each level represents a distinct security posture with specific requirements, features, and trade-offs. This document serves as the master reference for what security features are required or optional at each level.
  - L67: │  • Basic audit optional                                         │
  - L75: │  • Write-after log (WAL, optional extension) encryption                             │
  - L143: ¹ External audit storage may not be available in embedded mode
  - L169: ● = Required | ○ = Optional | ✓ = Allowed | (blank) = Not applicable/available
  - L195: | Write-after log (WAL, optional extension) encryption | | | ● | ● | ● | ● | ● |
  - L280: - SHOULD display warning on startup
  - L281: - SHOULD be disabled in production builds
  - L302: | Audit | Optional |
  - L338: | Audit | Optional |
  - L359: - HSM optional
  - L571: │  Note: Upgrade may require restart for some features            │
  - L601: │  Note: Some downgrades may require data migration               │
  - L729: - Write-after log (WAL, optional extension) is encrypted
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/10_RELEASE_INTEGRITY_PROVENANCE.md
- Potential ambiguity markers detected:
  - L37: - Community builds MAY exist but MUST NOT be represented as certified.
  - L45: Official builds SHOULD be reproducible.
  - L92: - Downgrades across security‑fix releases SHOULD be prevented by default.
  - L106: The runtime MAY expose the support classification via an engine‑owned virtual view.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/AUTH_CERTIFICATE_TLS.md
- Potential ambiguity markers detected:
  - L59: CLIENT_CERT_OPTIONAL,       // Client cert optional
  - L61: CLIENT_CERT_OPTIONAL_NO_CA  // Optional, don't verify CA
  - L228: // Check if we should override certain errors
  - L619: strcpy(ctx->ctx_error_msg, "OCSP status unknown");
  - L1248: client_cert_mode: required  # none, optional, required
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/EXTERNAL_AUTHENTICATION_DESIGN.md
- Potential ambiguity markers detected:
  - L207: 6. Synchronize with local catalog (optional)
  - L387: ### 🔮 optional extension
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/contributor_security_rules.md
- Potential ambiguity markers detected:
  - L30: - Only the **engine core** may make security decisions.
  - L42: The parser MAY:
  - L69: BeforeSelect hooks MAY:
  - L83: BeforeInsert hooks MAY:
  - L147: - Add partial or optional MEK behavior outside Level 6
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/sbsec_alpha_base.md
- Potential ambiguity markers detected:
  - L43: No other component may make or override security decisions.
  - L59: 9. Execute optional security hooks (see §5)
  - L72: Higher security modes may add additional enforcement hooks but MUST NOT violate transaction isolation semantics.
  - L80: BeforeSelect hooks MAY be configured for objects requiring immediate authorization re-evaluation.
  - L82: They MAY:
  - L133: - MEK is OPTIONAL for Security Levels 0–5
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/Security Design Specification/supportability_contract.md
- Potential ambiguity markers detected:
  - L29: Unsupported deployments MAY function but are not eligible for official support.
  - L79: - Operators MUST acknowledge that data loss may be irrecoverable
  - L117: - The issue MAY be escalated to deeper investigation
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/TEMPORARY_TABLES_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L12: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in V3. WAL is disabled for recovery; if enabled, it exists only as an optional replication/PITR stream.
  - L13: Any WAL references in this document describe an optional replication/PITR stream for
  - L136: - Temporary table data MUST NOT be written to the optional write-after log stream.
  - L140: - Engine MAY store temporary table data in memory or a dedicated temp tablespace.
  - L156: - Syntax: `CREATE TEMP[TEMPORARY] TABLE` with optional ON COMMIT.
  - L161: - TEMP table name SHOULD shadow permanent table names for that session.
  - L211: - Session-scoped temp tables MUST preserve definition; data MAY be cleared
  - L218: Minimum enforced rules (dialect-specific extensions MAY apply):
  - L244: SELECT * FROM session_temp;  -- should fail
  - L266: SELECT * FROM drop_on_commit;  -- should fail
  - L272: SELECT * FROM my_temp;        -- should fail
  - L282: - Storage: use temp tablespace or in-memory storage, no optional extension write-after log (WAL).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/UPDATE.md
- Potential ambiguity markers detected:
  - L14: 2. Parse target table reference and optional alias.
  - L16: 4. Parse optional FROM clause (if dialect permits).
  - L17: 5. Parse optional WHERE clause.
  - L18: 6. Parse optional RETURNING clause.
  - L46: - SET with unknown column: `ERR_COLUMN_NOT_FOUND`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/V2_PARSER_FIREBIRD_ALIGNMENT_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L19: > "The V2 parser which is the core parser of the project, will have many more expansions than any of the other three but in Style/Formatting it should follow the FirebirdSQL standard."
  - L76: // Conflict target (optional)
  - L120: // Optional WHERE clause for UPDATE
  - L294: -- Firebird UPDATE OR INSERT (V2 should support this)
  - L315: 4. Optional `RETURNING` clause returns affected row data
  - L334: // MATCHING clause (optional - defaults to primary key)
  - L399: -- RIGHT: Firebird style (V2 should use this)
  - L482: -- RIGHT: Firebird style (V2 should use this)
  - L544: -- V2 should follow Firebird semantics:
  - L677: // MATCHING clause (optional)
  - L687: // RETURNING clause (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/V2_PARSER_INDEX_TYPE_COMPLETENESS.md
- Potential ambiguity markers detected:
  - L8: types are core (non-optional) and must be implemented.
  - L84: else error("Unknown index type");
  - L285: ## Optional (Beta Scope)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md
- Potential ambiguity markers detected:
  - L91: - Executor applies session changes and may touch catalog state for persistent settings.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/V3_ZERO_AMBIGUITY_BUILD_CHECKLIST.md
- Potential ambiguity markers detected:
  - L30: - Collation runtime binary format is not defined in the authoritative V3 types set.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/WINDOWING.md
- Potential ambiguity markers detected:
  - L57: - `frame`: WINDOW_FRAME (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/admin/SB_ADMIN_CLI_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L323: # Write-after log    | N/A    | Optional (optional extension)
  - L424: | 3 | UNKNOWN | Check failed to execute |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/api/CONNECTION_POOLING_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L169: | **UserPool** | Per-user connection isolation (optional) |
  - L266: // 2. Check if connection should be closed
  - L307: When a connection is returned to the pool, it may need to be reset:
  - L771: // Execution plan (optional)
  - L1033: // Check MGA epoch (data may have changed)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/00-Implementation-Roadmap.md
- Potential ambiguity markers detected:
  - L149: - [ ] Protocol conformance tests (libpq/psql optional for validation)
  - L159: - [ ] Protocol conformance tests (mysql client/connector optional for validation)
  - L509: - Rebalance data (optional)
  - L652: **Target Start Date**: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/01-Architecture-Overview.md
- Potential ambiguity markers detected:
  - L20: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
  - L21: Any WAL references in this document describe an optional post-gold stream for
  - L132: | Storage, transactions, GC, maintenance | Primary | None | None | Firebird-style MGA; no write-after log (WAL, optional post-gold) in core. |
  - L201: │  ├─ Write-after log (WAL, optional post-gold) (durability)
  - L469: | **Bounded Staleness** | Reads may be up to Δt stale | Most transactions | 1x uncertainty |
  - L649: 3. Uncertainty may increase temporarily
  - L650: 4. Transactions may pause if strict mode enabled
  - L695: - **Programming Language**: C/C++ (engine core), Rust (optional parsers)
  - L708: - **Arrow** (optional, for OLAP columnar format)
  - L727: **Next Review Date**: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/02-Clock-Synchronization-Specification.md
- Potential ambiguity markers detected:
  - L430: log_warn("Clock master reports resync (time may have jumped)");
  - L921: # Time source (auto-detected if not specified)
  - L1006: **Next Review Date**: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/03-Distributed-MVCC-Specification.md
- Potential ambiguity markers detected:
  - L124: // Clock moved backward (should not happen with sync)
  - L259: // Optional: wait for uncertainty to pass
  - L548: // 4. Persist version (data pages; optional write-after log stream)
  - L1098: // Newer should point to older
  - L1198: **Next Review Date**: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/04-Replication-Protocol-Specification.md
- Potential ambiguity markers detected:
  - L131: // Optional: column-level changes for efficiency
  - L918: **Next Review Date**: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/05-Wire-Protocol-Integration-Specification.md
- Potential ambiguity markers detected:
  - L979: **Next Review Date**: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/08-Deployment-Guide.md
- Potential ambiguity markers detected:
  - L11: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
  - L12: Any WAL references in this document describe an optional post-gold stream for
  - L563: Special: GPS receiver (optional, for hardware sync)
  - L849: wal_dir: /var/lib/db/tx-engine/wal  # optional post-gold WAL only
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/10-UDR-System-Specification.md
- Potential ambiguity markers detected:
  - L966: status_set_error(status, -2, "Unknown function");
  - L1092: // 1. Check file permissions (should not be world-writable)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/11-Remote-Database-UDR-Specification.md
- Potential ambiguity markers detected:
  - L72: Optional (but recommended for full fidelity):
  - L492: -- Optional: Connection Pool
  - L499: -- Optional: SSL/TLS
  - L506: -- Optional: Query Behavior
  - L512: -- Optional: Health Check
  - L534: schema_name 'remote_schema',  -- Optional, default 'public'
  - L616: Optional overrides (server options):
  - L632: Live migration is optional and must be explicitly enabled per remote server.
  - L802: -- 5.7: Rename tables (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/11a-Connection-Pool-Implementation.md
- Potential ambiguity markers detected:
  - L48: Optional tuning:
  - L51: - session_init_sql (optional SQL executed on connect)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/11b-PostgreSQL-Client-Implementation.md
- Potential ambiguity markers detected:
  - L14: - Transport: TCP with optional TLS
  - L21: 2. Optional SSLRequest (int32 len=8, int32 code=80877103)
  - L35: - AuthenticationGSS/SSPI are not required for Alpha (optional)
  - L68: - Support text format; binary COPY is optional for Alpha
  - L84: - Severity INFO/NOTICE/WARNING should not fail the connection
  - L103: Unsupported types should be returned as STRING with a warning unless
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/11c-MySQL-Client-Implementation.md
- Potential ambiguity markers detected:
  - L15: - Transport: TCP with optional TLS
  - L26: - username, auth response, database (optional), auth plugin name
  - L27: 3. Optional TLS:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/11d-MSSQL-Client-Implementation.md
- Potential ambiguity markers detected:
  - L13: - Transport: TCP with optional TLS
  - L14: - Authentication: SQL Login (Windows/SSPI optional in Beta)
  - L27: - optional feature extensions
  - L45: - Cursor support is optional for Alpha
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/11e-Firebird-Client-Implementation.md
- Potential ambiguity markers detected:
  - L14: - Transport: TCP with optional encryption
  - L44: - Batch execution is optional for Alpha
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/11g-JDBC-Client-Implementation.md
- Potential ambiguity markers detected:
  - L23: - driverClass (optional if DriverManager auto-loads)
  - L30: - Scrollable ResultSet optional (TYPE_FORWARD_ONLY recommended)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/11h-Live-Migration-Emulated-Listener.md
- Potential ambiguity markers detected:
  - L17: | Snapshot | Consistent snapshot tied to CDC cursor | TODO |
  - L18: | CDC | Log-based CDC preferred, trigger fallback | TODO |
  - L19: | Ordering | Commit-order apply + atomic txns | TODO |
  - L20: | Schema Drift | Detection + pause + re-introspect | TODO |
  - L21: | Constraints | Enforce PK/UNIQUE/FK on apply | TODO |
  - L22: | LOBs | Full LOB snapshot + CDC updates | TODO |
  - L23: | Types | Charset/collation/time zone preservation | TODO |
  - L24: | Idempotency | Duplicate-safe apply via PK/unique | TODO |
  - L25: | Telemetry | sys.migration_status + audit log | TODO |
  - L26: | Cutover | Lag=0 + audit pass + sequence sync | TODO |
  - L27: | Rollback | Recorded window + divergence audit | TODO |
  - L28: | Security | TLS + secret storage + host allowlist | TODO |
  - L39: - Cutover and optional mirror-to-legacy rollback window.
  - L117: MIRROR_LEGACY (optional)
  - L131: | PRIMARY_EMULATED | Emulated | Emulated (+ optional mirror) | Optional | Emulated |
  - L132: | MIRROR_LEGACY | Emulated | Emulated + Legacy | Optional | Emulated |
  - L168: - **Sample compare** (optional)
  - L174: Audit output should be persisted for review, e.g. `sys.migration_audit_log`
  - L202: Cutover should only proceed when:
  - L297: - For idempotency, changes should be applied using primary keys and
  - ... 3 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/alpha_phase_2/README.md
- Potential ambiguity markers detected:
  - L289: - GPS receiver (optional, for hardware sync)
  - L548: [To be determined]
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/cluster/scratch_bird_cluster_architecture_security_specifications_draft.md
- Potential ambiguity markers detected:
  - L49: - Protocol / Capability Matrix (optional)
  - L87: Kerberos/GSSAPI MAY be supported as an additional enterprise integration, but MUST NOT replace mutual authentication.
  - L124: - Operators MAY force cutover (destructive, audited, explicit acknowledgement required)
  - L216: ### 8.2 Cluster‑Consistent Backup Sets (Optional)
  - L240: - LOCAL_SAFE may continue with bounded behavior
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/00_SECURITY_SPEC_INDEX.md
- Potential ambiguity markers detected:
  - L256: - **○** = Optional at this security level
  - L263: - Box with dashed lines: Optional component
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/01_SECURITY_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L40: The keywords MUST, MUST NOT, REQUIRED, SHALL, SHALL NOT, SHOULD, SHOULD NOT, RECOMMENDED, MAY, and OPTIONAL in this document are to be interpreted as described in RFC 2119.
  - L65: The Engine Core is the sole authority for all security decisions. No other component (parser, network listener, client, plugin) may bypass, override, or influence security enforcement.
  - L338: The Engine MAY send the following commands to the Network Listener:
  - L638: | Locks              | Sessions may block on locks but cannot see each other's lock state |
  - L684: c. May spawn replacement parser for pool
  - L689: Administrators may terminate sessions:
  - L705: Sessions may be terminated for security reasons:
  - L740: │  • Active role (from session, may be NONE)                       │
  - L899: Compiled SBLR may be cached for performance. Cache security:
  - L916: ### 8.2 Current Implementation (Alpha)
  - L1016: | Optional           | ●   |     |     |     |     |     |     |
  - L1035: ● = Required | ○ = Optional | (blank) = Not applicable
  - L1061: No parser, listener, plugin, or external component may bypass Engine enforcement.
  - L1065: > Sessions are completely isolated. No session may access another session's state, memory, or transaction context.
  - L1139: | Administrative session termination timeout | To be determined | Policy configuration    |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/02_IDENTITY_AUTHENTICATION.md
- Potential ambiguity markers detected:
  - L51: ● = Required | ○ = Optional | (blank) = Not applicable
  - L87: │  │  • Usage limits (optional)                              │    │
  - L88: │  │  • Allowed roles subset (optional)                      │    │
  - L165: - May be referenced in audit logs indefinitely after user deletion
  - L170: - Usernames may be changed without affecting permissions
  - L203: not_before          TIMESTAMP,              -- Optional: validity window start
  - L244: │       - Optional restrictions (roles, addresses)                 │
  - L247: │     • Session may be created using AuthKey                       │
  - L258: │     • No new sessions may use this AuthKey                       │
  - L321: A single AuthKey may be used to create multiple sessions (subject to max_uses):
  - L324: - Sessions may have different active roles (within AuthKey's allowed_roles)
  - L667: # Common password check (optional)
  - L802: | Registration | Only CLUSTER_ADMIN may register providers |
  - L804: | Signature | Optional cryptographic signature verification |
  - L1210: - Client should retry or fail
  - L1241: | CAPTCHA | Optional for repeated failures |
  - L1249: | Session binding | Optional IP/fingerprint binding |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/03_AUTHORIZATION_MODEL.md
- Potential ambiguity markers detected:
  - L53: ● = Required | ○ = Optional | (blank) = Not applicable
  - L591: │     • May expand wildcards (ALL TABLES IN SCHEMA)                │
  - L743: Certain users may bypass RLS:
  - L975: Users may need to access objects across dialects:
  - L1090: │  • Session may be terminated                                     │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/04_ENCRYPTION_KEY_MANAGEMENT.md
- Potential ambiguity markers detected:
  - L54: ● = Required | ○ = Optional | (blank) = Not applicable
  - L710: │  ├─ Wrapped BKK (passphrase) - optional                        │
  - L901: │  • Pause during peak hours: optional                            │
  - L1040: | Memory scraping | MEK, secure memory allocation, optional memory encryption |
  - L1122: | `wire.client_cert_mode` | enum | DISABLED | DISABLED, OPTIONAL, REQUIRED |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/05_IPC_SECURITY.md
- Potential ambiguity markers detected:
  - L61: ● = Required | ○ = Optional | (blank) = Not applicable
  - L775: | 1002 | INVALID_TOKEN | Unknown or expired token |
  - L783: | 2001 | INTERNAL_ERROR | Unspecified internal error |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/06_CLUSTER_SECURITY.md
- Potential ambiguity markers detected:
  - L53: ● = Required | ○ = Optional | (blank) = Not applicable
  - L78: │  • May also be a Data Node                                      │
  - L212: │  • Reads may go to any Security Node (with replication lag)     │
  - L641: │  Optional Hard Fencing (STONITH):                               │
  - L815: │  • Unknown users cannot authenticate                            │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/07_NETWORK_PRESENCE_BINDING.md
- Potential ambiguity markers detected:
  - L49: - **OPTIONAL** at Security Level 5
  - L597: Shares should be periodically refreshed without changing the secret:
  - L711: • Admin should replace failed node
  - L712: • Optional: refresh shares to exclude Node 3's share
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/08_AUDIT_COMPLIANCE.md
- Potential ambiguity markers detected:
  - L52: ● = Required | ○ = Optional | (blank) = Not applicable
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/09_SECURITY_LEVELS.md
- Potential ambiguity markers detected:
  - L19: This document defines the seven security levels (0-6) supported by ScratchBird. Each level represents a distinct security posture with specific requirements, features, and trade-offs. This document serves as the master reference for what security features are required or optional at each level.
  - L59: │  • Basic audit optional                                         │
  - L135: ¹ External audit storage may not be available in embedded mode
  - L161: ● = Required | ○ = Optional | ✓ = Allowed | (blank) = Not applicable/available
  - L272: - SHOULD display warning on startup
  - L273: - SHOULD be disabled in production builds
  - L294: | Audit | Optional |
  - L330: | Audit | Optional |
  - L351: - HSM optional
  - L563: │  Note: Upgrade may require restart for some features            │
  - L593: │  Note: Some downgrades may require data migration               │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/Beta Task -Distributed Secret Sharing Implementation Specification.md
- Potential ambiguity markers detected:
  - L94: The transition from mathematical theory to C++ implementation requires rigorous attention to memory management. Standard containers like std::vector and std::string are designed for performance and convenience, not security. They may leave copies of data in memory after reallocation, or "optimize away" attempts to clear memory.14
  - L101: 2. **Locking (Optional but Recommended):** On supported OSs, mlock (Linux) or VirtualLock (Windows) should be used to prevent the OS from swapping sensitive memory pages to the hard disk.
  - L105: Using memset to clear secrets is unsafe. Compilers analyzing data flow may determine that the memory is about to be freed and that the write is "dead" (has no observable effect). Consequently, the Dead Store Elimination (DSE) optimization pass will remove the memset call entirely.14
  - L107: **Solution:** We use a volatile pointer cast or explicit compiler barriers. The volatile keyword tells the compiler that the memory access has side effects unknown to it, preventing the optimization.16
  - L201: \#**include** \<optional\>
  - L217: virtual std::optional\<Types::SecureByteBlock\> Join(
  - L383: std::optional\<SecureByteBlock\> Join(const std::vector\<Share\>& shares) override {
  - L624: * The Fix: The specification must explicitly state that the Local Share must be sealed in the TPM (Trusted Platform Module) or not stored at all (stateless nodes). If TPM is not available, the node should hold *no* share of its own key, and rely 100% on peers (requiring $k$ remote peers).
  - L661: * The Fix: The spec should recommend C11 memset\_s or OS-specific SecureZeroMemory (Windows) / explicit\_bzero (Linux).
  - L847: if (a \== 0\) return 0; // Mathematically undefined, but 0 is safe for SSS logic checks
  - L919: \#include \<optional\>
  - L1002: std::optional\<SecureByteBlock\> Join(const std::vector\<Share\>& shares) {
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/SECURITY_IMPLIMENTATION_DETAILS.md
- Potential ambiguity markers detected:
  - L341: - Optional  cryptographic signature verification for distribution builds
  - L385: - auto_revoke:  Remove role if group no longer present (optional)
  - L530: - client_cert_mode:  DISABLED, OPTIONAL, or REQUIRED
  - L606: Tokens:** Optional
  - L908: -- Cached permission snapshot (optional)
  - L1020: **Sliding Expiration (Optional):** Each access extends expiration. Better availability but risk of indefinitely caching stale data.
  - L1263: -- Optional: periodic version check
  - L1413: -- Cache tables should be:
  - L1601: library_signature       BYTEA,  -- optional PGP/X.509 signature
  - L1722: /* User identification (may be partially filled) */
  - L1724: uuid_t          user_uuid;          /* looked up, may be NULL_UUID if unknown */
  - L1795: AUTH_USER_NOT_FOUND     = 2,    /* user unknown to provider */
  - L1867: *   May be called concurrently; must be thread-safe.
  - L1924: /* Optional functions (NULL if not implemented) */
  - L2240: │      - If TRUE: continue (provider may auto-create)             │
  - L2404: -- Don't continue; user should exist in specified provider
  - L2677: | `auth.allow_unknown_user_to_provider`            | BOOLEAN | FALSE      | Let provider handle unknown users  |
  - L2679: | `auth.fallback_on_user_not_found`                | BOOLEAN | TRUE       | Try next provider if user unknown  |
  - L3313: /* May need to fetch old key version for rotation-in-progress */
  - L3446: │ 5. DESTROY OLD VERSION (optional, after retention period)       │
  - ... 17 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/SECURITY_SYSTEM_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L16: **Legacy term note:** This document may use xmin/xmax labels for creator/deleter transaction IDs. ScratchBird does not use PostgreSQL tuple headers; see the authoritative MGA specs above for the actual Firebird-style record header fields (e.g., `rhd_transaction`, `rhd_back_version`) and visibility/GC rules.
  - L77: - Optional **information_schema views** for tool compatibility
  - L854: **Optional Compatibility Views**: Read-only `information_schema` views for tool compatibility.
  - L2025: - Kerberos authenticator (optional)
  - L2026: - Active Directory authenticator (optional)
  - L2040: ### Phase 4: Optional Features (100-150 hours) - Future
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/Security Hardening Guide.md
- Potential ambiguity markers detected:
  - L231: **Write-after log (WAL) data remnants undermine secure deletion**. SQLite and PostgreSQL write-after log (WAL) files retain deleted data until checkpoint operations—forensic tools recover "securely deleted" records from write-after log (WAL) slack space. GDPR Article 17 requires erasure "without undue delay," but write-after log (WAL) remnants may persist indefinitely. Rollback journals in TRUNCATE mode reset file size to 0 while content remains recoverable from disk sectors.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/archive/security/draft_security_architecture_specification.md
- Potential ambiguity markers detected:
  - L39: - Parsers may cache parse results but do **not** enforce authorization.
  - L64: - A connection may host multiple sessions.
  - L121: - Plans may be cached only within the same session and security context.
  - L134: - Dynamic SQL may be globally disabled by engine configuration.
  - L158: - Domains may define:
  - L175: - May be compressed and encrypted during generation.
  - L177: - May operate in user-scoped or privileged modes.
  - L189: - Databases may act as:
  - L232: - Authentication optional or disabled
  - L335: - Migration pass-through may inherit legacy security weaknesses
  - L345: All authorization, dependency verification, and execution approval decisions are made by the engine. Parsers, listeners, and plugins may not bypass engine enforcement.
  - L348: No execution context may access any database object whose UUID is not present in the approved dependency set for the transaction.
  - L357: Plugins may only perform actions explicitly granted by their declared and approved capability set.
  - L579: **Optional fields**
  - L591: - A BreakGlassToken may only be attached to a session by an identity with explicit break-glass activation permission.
  - L602: - In-flight statements may be cancelled (policy), or allowed to complete but subsequent statements denied.
  - L606: - Tokens may be revoked early by quorum-approved action or by designated security controllers.
  - L607: - Revocation takes effect at the next statement boundary at latest; higher modes may enforce immediate cancellation.
  - L622: - Break-glass may not disable audit logging.
  - L623: - Break-glass may not reduce encryption requirements.
  - ... 6 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md
- Potential ambiguity markers detected:
  - L16: This directory contains specification directories for all drivers, integrations, and tools that ScratchBird must support to achieve broad market adoption. Each subdirectory should contain detailed specifications, implementation plans, and testing criteria for that specific driver or integration.
  - L20: - **P1 (High)**: Should have for Beta - significant user base
  - L21: - **P2 (Medium)**: Nice to have for Beta or optional extension
  - L386: ### Phase 2: optional extension Expansion (P1 - High Priority)
  - L388: **Should Have:**
  - L409: Each driver/integration directory should contain:
  - L423: Each SPECIFICATION.md should include:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/COMPLETION_STATUS.md
- Potential ambiguity markers detected:
  - L47: ### optional extension (P3) Track
  - L61: | **Unspecified** | 10 | - | - | - | - |
  - L80: | **optional** | 2 | 0 | 0 | 0 | 0 | 0 | 2 | 0% |
  - L146: ### Optional (0% complete)
  - L211: | Unspecified | 10 | - | - |
  - L222: | Optional | 0% (0/2) | ⏳ |
  - L244: - All P1 items should be at least **Partial** 🚧
  - L245: - P2 items are optional extension
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/DIRECTORY_STRUCTURE_CREATED.md
- Potential ambiguity markers detected:
  - L168: **Should have for Beta or shortly after**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/README.md
- Potential ambiguity markers detected:
  - L143: ### Optional (Beta Engine Features)
  - L145: **[optional/](optional/)** - Optional beta engine features
  - L147: - [STORAGE_ENCODING_OPTIMIZATIONS.md](optional/STORAGE_ENCODING_OPTIMIZATIONS.md) - Varlen header v2, per-column TOAST, packed NUMERIC
  - L148: - [AUDIT_TEMPORAL_HISTORY_ARCHIVE.md](optional/AUDIT_TEMPORAL_HISTORY_ARCHIVE.md) - Optional GC-backed audit + temporal history archive
  - L155: | **P1 (High)** | Should have for Beta - significant user base | Beta or shortly after |
  - L156: | **P2 (Medium)** | Nice to have for Beta or optional extension | optional extension |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/big-data-streaming/apache-kafka/README.md
- Potential ambiguity markers detected:
  - L21: 3. **Connectors:** optional Kafka Connect source/sink adapters.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/big-data-streaming/apache-kafka/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L34: - **DDL events**: schema changes (optional channel)
  - L41: - **Audit rehydration** (optional)
  - L43: ### 3) Kafka Connect Adapters (Optional)
  - L55: **Optional encodings**: Protobuf or JSON
  - L65: - `payload_before` (optional)
  - L66: - `payload_after` (optional)
  - L73: DDL events are optional but recommended for downstream consumers.
  - L191: - [ ] Add DDL event channel (optional)
  - L195: - [ ] Provide Kafka Connect source/sink adapters (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/00_BUILD_REQUIREMENTS_INDEX.md
- Potential ambiguity markers detected:
  - L85: ### Optional Dependencies
  - L87: - **GEOS:** Spatial geometry (optional)
  - L88: - **PROJ:** Geographic projections (optional)
  - L89: - **libxml2:** XML support (optional)
  - L118: - Wine for testing (optional but recommended)
  - L204: Each build requirements document should follow this structure:
  - L210: 5. **Optional Dependencies**: Optional features
  - L222: These documents should be updated:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/01_LINUX_NATIVE.md
- Potential ambiguity markers detected:
  - L102: ## 5. Optional Dependencies
  - L104: ### 5.1 Spatial Support (Optional)
  - L111: ### 5.2 XML Support (Optional)
  - L117: ### 5.3 Development Tools (Optional but Recommended)
  - L158: # Install optional dependencies
  - L164: # Install development tools (optional)
  - L197: # Install optional dependencies
  - L203: # Install development tools (optional)
  - L235: # Install optional dependencies
  - L241: # Install development tools (optional)
  - L273: # Install optional dependencies
  - L279: # Install development tools (optional)
  - L298: gcc --version    # Should show 11.0 or later
  - L299: g++ --version    # Should show 11.0 or later
  - L302: clang --version    # Should show 14.0 or later
  - L303: clang++ --version  # Should show 14.0 or later
  - L309: cmake --version  # Should show 3.20 or later
  - L324: pkg-config --modversion spdlog    # Should show 1.10.0+
  - L325: pkg-config --modversion openssl   # Should show 1.1.1+ or 3.0+
  - L326: pkg-config --modversion lz4       # Should show 1.9.3+
  - ... 2 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/02_WINDOWS_NATIVE.md
- Potential ambiguity markers detected:
  - L64: - C++ Clang tools for Windows (optional, for Clang/LLVM)
  - L77: **Alternative: Ninja** (Optional, for faster builds):
  - L141: ## 6. Optional Dependencies
  - L143: ### 6.1 Spatial Support (Optional)
  - L150: ### 6.2 XML Support (Optional)
  - L184: cmake --version  # Should show 3.20 or later
  - L200: # Install optional dependencies
  - L225: # Should display Microsoft C/C++ Optimizing Compiler version
  - L231: cmake --version  # Should show 3.20 or later
  - L237: git --version  # Should show 2.30 or later
  - L259: # Should show installed versions
  - L307: ### 9.4 Configuration with Optional Features
  - L518: - Visual Assist (optional, code navigation)
  - L519: - ReSharper C++ (optional, refactoring)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/03_MACOS_NATIVE.md
- Potential ambiguity markers detected:
  - L113: brew --version  # Should show Homebrew 4.0+
  - L152: ## 6. Optional Dependencies
  - L154: ### 6.1 Spatial Support (Optional)
  - L161: ### 6.2 XML Support (Optional)
  - L178: xcode-select -p  # Should show: /Library/Developer/CommandLineTools
  - L181: clang --version  # Should show Apple clang 13.0+
  - L210: # Install Ninja (optional, for faster builds)
  - L227: # Install optional dependencies
  - L241: clang --version   # Should show Apple clang 13.0+
  - L242: clang++ --version # Should show Apple clang 13.0+
  - L253: cmake --version  # Should show 3.20 or later
  - L351: ### 9.5 Configuration with Optional Features
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/10_LINUX_TO_WINDOWS.md
- Potential ambiguity markers detected:
  - L98: ## 5. Optional Dependencies (Windows Target)
  - L100: ### 5.1 Spatial Support (Optional)
  - L107: ### 5.2 XML Support (Optional)
  - L337: x86_64-w64-mingw32-gcc --version    # Should show GCC 11.0+
  - L338: x86_64-w64-mingw32-g++ --version    # Should show GCC 11.0+
  - L348: cmake --version  # Should show 3.20 or later
  - L351: ### 9.3 Verify Wine (Optional, for Testing)
  - L354: wine64 --version  # Should show Wine 6.0+
  - L373: wine64 test.exe  # Should print "Hello from Windows!"
  - L540: # Issue: undefined reference to WinMain
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/11_LINUX_TO_MACOS.md
- Potential ambiguity markers detected:
  - L541: # Should show: Architectures in the fat file: x86_64 arm64
  - L546: ## 13. Code Signing (Optional)
  - L641: # Should show: Mach-O universal binary with 2 architectures
  - L645: # Should show: x86_64 arm64
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/12_WINDOWS_TO_LINUX.md
- Potential ambiguity markers detected:
  - L85: # Should show Version 2004 (Build 19041) or later
  - L133: # Should show VERSION 2 for your distribution
  - L221: # Install optional dependencies
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/20_APPIMAGE.md
- Potential ambiguity markers detected:
  - L68: ### 3.2 Optional Tools
  - L215: **AppRun Script** (optional, for custom startup):
  - L274: Some libraries should NOT be bundled (provided by all systems):
  - L329: ### 8.3 Sign AppImage (Optional)
  - L437: # All paths should be relative to $ORIGIN or system libraries
  - L649: # Should match your system (x86-64 or aarch64)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/23_DEB.md
- Potential ambiguity markers detected:
  - L107: │   ├── scratchbird.service  # Systemd service file (optional)
  - L127: Priority: optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/27_BREW.md
- Potential ambiguity markers detected:
  - L142: depends_on "python@3.11" => :optional  # Optional dependency
  - L413: ├── Casks/              # Optional: for .app bundles
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/30_DOCKER.md
- Potential ambiguity markers detected:
  - L28: | **Docker Compose** | 2.0.0 or later (optional) |
  - L108: docker --version  # Should show 20.10.0+
  - L679: # (default if USER not specified)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/builds/COMPLETE_BUILD_ENVIRONMENT_SETUP.md
- Potential ambiguity markers detected:
  - L153: # SECTION 7: Optional Dependencies
  - L472: # SECTION 7: Optional Dependencies
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/cloud-container/docker/README.md
- Potential ambiguity markers detected:
  - L167: linux/arm/v7   - ARMv7, Raspberry Pi (optional)
  - L331: # Write-after log (WAL) and checkpointing (optional, optional extension)
  - L358: - Configuration volume (optional)
  - L359: - Log volume (optional)
  - L360: - Backup volume (optional)
  - L688: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/cloud-container/docker/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L40: - Distroless (optional)
  - L71: - Optional protocol ports if enabled (pg/mysql/firebird)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/connectivity/jdbc/README.md
- Potential ambiguity markers detected:
  - L12: - **Authentication:** Username/password; optional TLS.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/connectivity/odbc/README.md
- Potential ambiguity markers detected:
  - L18: **Scope Note:** SQL Server ODBC comparisons and SSRS references are optional extension; MSSQL/TDS emulation is not part of current scope.
  - L78: - From SQL Server ODBC (optional extension reference)
  - L372: | Power BI DirectQuery | Within 20% of SQL Server ODBC (optional extension reference) |
  - L480: - [ ] SQL Server Reporting Services (SSRS) (optional extension reference)
  - L595: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/connectivity/odbc/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L27: - MSSQL/TDS emulation (optional extension)
  - L51: **Optional (P1):**
  - L101: - Use SQLSTATE codes (HYC00 for optional features not implemented)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md
- Potential ambiguity markers detected:
  - L11: - **Transport**: TCP (default port 3092) and optional Unix domain socket where supported by the OS.
  - L30: - **Optional**: `SBLR_EXECUTE` for precompiled bytecode if the driver implements it.
  - L64: - Pooling: prefer language-standard pools; drivers without standard pools ship optional pooling disabled by default.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/README.md
- Potential ambiguity markers detected:
  - L135: ### Phase 3 (optional extension)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/cpp/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L86: ### Optional parameters
  - L114: | JSON | std::string | optional json library |
  - L200: - Optional compression negotiation (zstd).
  - L201: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/API_REFERENCE.md
- Potential ambiguity markers detected:
  - L17: - DbProviderFactory.CreateDataAdapter() (optional)
  - L27: - ChangeDatabase(name) (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/README.md
- Potential ambiguity markers detected:
  - L21: **Scope Note:** SQL Server migration references are informational; MSSQL/TDS emulation is optional extension.
  - L108: - From System.Data.SqlClient (SQL Server, optional extension reference)
  - L550: - (Optional) Microsoft.Extensions.Logging.Abstractions
  - L718: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/dotnet-csharp/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L20: - API: ADO.NET provider with optional EF Core integration.
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L34: - DbDataAdapter CreateDataAdapter() (optional)
  - L45: - void ChangeDatabase(string database) (optional)
  - L93: ### Optional parameters
  - L130: | GEOMETRY | ScratchBirdGeometry | optional NetTopologySuite |
  - L154: | JSON | string/JsonDocument | optional System.Text.Json |
  - L209: - Optional compression negotiation (zstd).
  - L210: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/golang/API_REFERENCE.md
- Potential ambiguity markers detected:
  - L25: - ResetSession(ctx) error (optional)
  - L43: - Optional column methods: ColumnTypeDatabaseTypeName, ColumnTypeNullable,
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/golang/README.md
- Potential ambiguity markers detected:
  - L548: Optional:
  - L574: golang.org/x/crypto v0.x.x // optional, for auth
  - L715: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/golang/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L20: - API: database/sql driver with optional sqlx and GORM support.
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L27: Unsupported features should return driver.ErrSkip or a descriptive error.
  - L40: - ResetSession(ctx context.Context) error (optional but preferred)
  - L58: - Optional: NextResultSet(), ColumnTypeDatabaseTypeName(), ColumnTypeNullable(),
  - L70: ### Optional parameters
  - L183: - Optional compression negotiation (zstd).
  - L184: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/API_REFERENCE.md
- Potential ambiguity markers detected:
  - L29: - prepareCall(sql) (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/README.md
- Potential ambiguity markers detected:
  - L495: ### Optional Dependencies
  - L621: - [ ] OSGi manifest (optional)
  - L682: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/java-jdbc/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L48: - CallableStatement prepareCall(String sql) (optional)
  - L94: ### Optional parameters
  - L124: | JSON | String | optional JSON lib |
  - L155: | JSON | LONGVARCHAR | String | optional JSON library |
  - L170: | VECTOR | OTHER | float[] | optional custom vector |
  - L210: - Optional compression negotiation (zstd).
  - L211: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/README.md
- Potential ambiguity markers detected:
  - L38: - Browser-compatible client (optional, for edge computing)
  - L267: const user: User | undefined = result.rows[0];
  - L468: - (Optional) Type conversion libraries
  - L692: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/nodejs-typescript/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L85: ### Optional parameters
  - L198: - Optional compression negotiation (zstd).
  - L199: - Optional SBLR execution for repeated queries if supported.
  - L208: - Pooling: optional Pool class; off unless explicitly used.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/README.md
- Potential ambiguity markers detected:
  - L538: - Optional: Visual Studio (for Windows DLL components)
  - L709: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/pascal-delphi/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L27: adapter should provide equivalent operations for connect, execute, and read.
  - L57: ### Optional parameters
  - L116: | JSON | string/TJSONObject | optional JSON library |
  - L171: - Optional compression negotiation (zstd).
  - L172: - Optional SBLR execution for repeated queries if supported.
  - L181: - Pooling: optional external pool component; driver does not pool by default.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/php/README.md
- Potential ambiguity markers detected:
  - L444: - (Optional) mysqli extension compatibility
  - L456: - May have slightly lower performance
  - L634: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/php/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L69: ### Optional parameters
  - L122: | XML | string | optional SimpleXML |
  - L174: - Optional compression negotiation (zstd).
  - L175: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/python/API_REFERENCE.md
- Potential ambiguity markers detected:
  - L54: ## Async API (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/python/README.md
- Potential ambiguity markers detected:
  - L209: threadsafety = 2  # Threads may share the module and connections
  - L248: def fetchone() -> Optional[Sequence]: ...
  - L278: - (Optional) NumPy >= 1.20 (for vector operations)
  - L279: - (Optional) Pandas >= 1.3 (for DataFrame integration)
  - L287: ### Optional Dependencies
  - L372: - [ ] Conda package (optional)
  - L431: **Assigned To:** TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/python/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L20: - API: PEP 249 DB-API 2.0 driver with optional asyncio.
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L26: The driver must implement the PEP 249 interfaces. Optional features must raise
  - L71: ### Async API (optional)
  - L94: ### Optional parameters
  - L126: | VECTOR | list[float]/numpy.ndarray | optional |
  - L128: | GEOMETRY | scratchbird.types.Geometry | optional shapely |
  - L144: | XML | str | optional ElementTree parsing |
  - L156: | VECTOR | list[float]/numpy.ndarray | optional |
  - L160: | MACADDR/MACADDR8 | bytes/str | optional netaddr.EUI |
  - L196: - Optional compression negotiation (zstd).
  - L197: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/r/API_REFERENCE.md
- Potential ambiguity markers detected:
  - L30: dbListTables(con)        # optional
  - L31: dbListFields(con, name)  # optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/r/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L44: dbListTables(con)        # optional
  - L45: dbListFields(con, name)  # optional
  - L57: ### Optional parameters
  - L117: | JSON | list/character | jsonlite optional |
  - L172: - Optional compression negotiation (zstd).
  - L173: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/ruby/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L68: ### Optional parameters
  - L101: | GEOMETRY | Scratchbird::Geometry | optional RGeo |
  - L180: - Optional compression negotiation (zstd).
  - L181: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/drivers/rust/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L23: - Transport: TCP with TLS 1.3; optional Unix socket where supported.
  - L82: ### Optional parameters
  - L195: - Optional compression negotiation (zstd).
  - L196: - Optional SBLR execution for repeated queries if supported.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_CATALOG_MODEL_SPEC.md
- Potential ambiguity markers detected:
  - L32: - **Bucket**: Key-value namespace with optional TTL and value log settings.
  - L205: Each object type should be visible via `sys.*` monitoring views (required):
  - L212: Additional runtime metrics (counts, TTL expirations, compaction) should be
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_ENGINE_TYPE_OVERVIEW.md
- Potential ambiguity markers detected:
  - L21: deciding which NoSQL paradigms ScratchBird should emulate or integrate.
  - L260: with optional metadata filters.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_LANGUAGE_SPEC_TRACKER.md
- Potential ambiguity markers detected:
  - L11: **Reject policy (mandatory):** All NoSQL dialects are optional extensions.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_SCHEMA_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L61: ### 4.2 sys.nosql_collection_indexes (optional helper)
  - L87: - `expires_at` TIMESTAMP (optional)
  - L106: ### 6.2 sys.nosql_cf_columns (optional helper)
  - L182: - `graph_id` UUID (optional for named graphs)
  - L246: ### 11.2 sys.vector_indexes (optional helper)
  - L283: - Specialized storage modes are optional and can be introduced incrementally
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/NOSQL_STORAGE_STRUCTURES_REPORT.md
- Potential ambiguity markers detected:
  - L75: - Optional change stream log (append-only history)
  - L85: - Change stream or audit-friendly append log (optional, Beta)
  - L91: - Value log for large values (optional)
  - L102: - Optional value-log layout for large values (if needed)
  - L208: These show up across multiple NoSQL models and should be required as shared
  - L215: - **Change streams / CDC** for document and key-value use cases (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/languages/README.md
- Potential ambiguity markers detected:
  - L10: for NoSQL dialects that ScratchBird may emulate. Each language spec is intended
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/languages/cypher/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L18: - **Optional matching**: OPTIONAL MATCH yields nulls when no match exists.
  - L43: optional_match_clause = "OPTIONAL" "MATCH" pattern [ where_clause ] ;
  - L121: - **OPTIONAL MATCH** is a left-outer match; unbound variables become null.
  - L148: **Optional match**
  - L151: OPTIONAL MATCH (u)-[:HAS_PROFILE]->(p:Profile)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/languages/elasticsearch_dsl/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L57: | "should" ":" query_list
  - L96: - **Bool**: `must` and `filter` clauses are ANDed; `should` is OR with
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/languages/gremlin/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L18: - **Side effects**: Steps may produce side effects (aggregates, sacks).
  - L88: - Steps may filter traversers, expand them (graph traversal), or aggregate.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/languages/hbase_shell/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L69: - Filters may be combined with AND/OR semantics via `FilterList`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/nosql/languages/sparql/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L19: - **Graph pattern**: Combination of triple patterns with OPTIONAL, UNION, etc.
  - L68: optional_block = "OPTIONAL" group_graph_pattern ;
  - L101: - **OPTIONAL**: Left-outer join; missing matches yield unbound variables.
  - L130: **Optional + union**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/optional/AUDIT_TEMPORAL_HISTORY_ARCHIVE.md
- Potential ambiguity markers detected:
  - L1: # Audit + Temporal History Archive (Optional Beta)
  - L29: Provide an **optional Beta implementation** that combines temporal tables and audit logging to support **long-term, high-assurance user activity tracking** (DML over time) with **GC-assisted archival** of historical versions and audit records.
  - L61: This spec adds **optional enrichment** of those history rows with:
  - L83: - **External sinks** (S3, WORM, syslog/Kafka; optional)
  - L89: ### 5.1 Temporal History Columns (optional)
  - L113: Optional archive tables mirror the active audit/history tables.
  - L142: Add **optional GC archival policies**:
  - L152: # Optional audit archival
  - L165: Audit chain should span both live and archived events:
  - L197: ## 8. Trigger Integration (Optional)
  - L213: - Optional external WORM storage recommended for Level 6.
  - L225: ## 11. Acceptance Criteria (Optional Beta)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/optional/README.md
- Potential ambiguity markers detected:
  - L1: # Optional Beta Specifications
  - L16: This directory holds optional beta-phase engine features that are not required for the initial beta release but are approved for implementation during beta.
  - L25: - These specs should not override Alpha or core storage specs unless the table storage_format is explicitly set to v2.
  - L26: - Add new optional specs here and link them in this index.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/optional/STORAGE_ENCODING_OPTIMIZATIONS.md
- Potential ambiguity markers detected:
  - L1: # Beta Optional: Storage Encoding Optimizations (Varlen, Numeric, TOAST)
  - L10: ScratchBird v1 uses a fixed 4-byte length prefix for all variable-length values and currently TOASTs whole tuples. Firebird uses a 2-byte length for VARCHAR, and PostgreSQL uses a 1-byte header for short varlena values plus TOAST for large attributes. This optional beta feature reduces per-value overhead for short strings/binary, avoids toasting entire rows, and optionally adds a packed NUMERIC encoding for arbitrary precision.
  - L15: - Provide an optional packed NUMERIC encoding for precision beyond scaled-int limits.
  - L42: ## Proposed v2 Changes (Optional)
  - L86: Column parameters override table defaults. Unspecified parameters inherit from table defaults.
  - L181: - `COMPRESSED`: compress inline (optional)
  - L192: ### 4) Packed NUMERIC Encoding (Optional)
  - L193: Provide an optional packed NUMERIC encoding for arbitrary precision values, based on PostgreSQL's base-10000 digit layout.
  - L232: - NUMERIC with precision <= 38 may still use scaled-int unless configured to packed.
  - L234: ### 5) Compression (Optional)
  - L235: Compression is optional and may be applied inline or in TOAST storage.
  - L239: - Compression algorithms are defined in `COMPRESSION_FRAMEWORK.md` (LZ4 baseline, Zstd optional).
  - L250: ## Implementation Checklist (Beta Optional)
  - L255: - Implement packed NUMERIC encoder/decoder (optional behind config).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/optional/TABLESPACE_SHRINK_COMPACTION.md
- Potential ambiguity markers detected:
  - L1: # Optional Beta Specification: Tablespace Shrink and Compaction
  - L16: Status: Optional Beta
  - L29: - Optional offline compaction (admin-only) for faster, simpler operation.
  - L52: ### 4.2 Offline (optional)
  - L96: **Optional aggregate:** provide a tablespace-level summary by aggregating
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/BETA_REPLICATION_ARCHITECTURE_FINDINGS.md
- Potential ambiguity markers detected:
  - L16: **Legacy term note:** This document may use xmin/xmax labels for creator/deleter transaction IDs. ScratchBird does not use PostgreSQL tuple headers; see the authoritative MGA specs above for the actual Firebird-style record header fields (e.g., `rhd_transaction`, `rhd_back_version`) and visibility/GC rules.
  - L29: **Key Finding:** Specifications are OUT OF DATE. The codebase has a fully implemented transaction system (~2,069 lines) with TIP, MGA visibility, group commit, and 2PC support. Beta replication specs should build on this reality, not outdated design docs.
  - L269: **Proposed:** Add optional time-partitioned Merkle verification to existing LSN-based catch-up
  - L403: **P1 (Should-Have for Beta):**
  - L545: **Concern:** Changing UUIDv7 format may break compatibility
  - L565: Beta replication should **extend this foundation**, not reinvent it.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/REPLICATION_AND_SHADOW_PROTOCOLS.md
- Potential ambiguity markers detected:
  - L16: **Scope Note:** ScratchBird uses MGA and does not use WAL for recovery. References to WAL here describe an optional write-after log stream for replication/PITR (optional extension).
  - L69: - May need complete rebuild
  - L79: - May lag during high load
  - L253: - May timeout waiting
  - L466: - Simple but may lose updates
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/WAL_IMPLEMENTATION.md
- Potential ambiguity markers detected:
  - L18: This document provides the complete specification for the write-after log (WAL) system in ScratchBird. The write-after log (WAL) stream is an optional replication/PITR mechanism and is **not** used for local crash recovery (MGA provides recovery semantics).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/00_BETA_REPLICATION_INDEX.md
- Potential ambiguity markers detected:
  - L102: **Reject policy (mandatory):** Replication is an optional extension. If replication
  - L645: - Actual implementation may take longer (acceptable)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/00_REPLICATION_INDEX.md
- Potential ambiguity markers detected:
  - L76: **Reject policy (mandatory):** Replication is an optional extension. If replication
  - L284: - Saga pattern for cross-partition transactions (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/01_CORE_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L142: T4: Node B rejects write (unknown column 'age')
  - L164: primary: node_uuid_1 (for write-after log (WAL) streaming, optional in leaderless mode)
  - L449: Step 5: Add New Replica (Optional)
  - L555: - If T=1000 is between commit of users and orders, results may be inconsistent
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/01_UUIDV8_HLC_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L138: - Optional: Can be set to 0 if microsecond precision not available
  - L509: // Final tiebreak: random bits (should never reach here in practice)
  - L741: auto uuid2 = gen.generate();  // Should use ts=1000, counter=1
  - L777: // Node B generates UUID (should be > A's)
  - L875: - [ ] Conflict log (optional: store discarded versions for user review)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/02_LEADERLESS_QUORUM_REPLICATION.md
- Potential ambiguity markers detected:
  - L469: - Guarantees: Read may see stale data (older version)
  - L625: - **Replicas**: Some may have applied write, some may not
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/03_SCHEMA_DRIVEN_COLOCATION.md
- Potential ambiguity markers detected:
  - L295: // Optional: Explicit colocation group
  - L296: std::optional<std::string> colocation_group;
  - L315: LOG_WARNING("Partition key {} not in primary key (may cause hot spots)",
  - L396: **Problem**: User may write transaction that spans multiple shards (unknowingly or intentionally).
  - L412: // Partition key not specified → Full table scan (all shards)
  - L573: ### Two-Phase Commit (Optional)
  - L632: **Problem**: Users may not know optimal partition key for colocation.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/04_TIME_PARTITIONED_MERKLE_FOREST.md
- Potential ambiguity markers detected:
  - L60: **Cause**: In leaderless quorum replication (Mode 3), replicas may diverge due to:
  - L268: // 4. Optional: Serialize tree to disk, free memory
  - L297: // 4. Check if previous window should be sealed
  - L546: 3. Recompute Window 5 root hash → Should now match Node B
  - L622: **Trade-off**: Probabilistic verification (may miss some divergences), but low overhead.
  - L867: **Compression**: Optional gzip compression for large forests (e.g., > 1000 trees).
  - L909: // Insert more rows in Window 1 (should not affect Window 0)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/05_MGA_INTEGRATION.md
- Potential ambiguity markers detected:
  - L16: **Legacy term note:** This document may use xmin/xmax labels for creator/deleter transaction IDs. ScratchBird does not use PostgreSQL tuple headers; see the authoritative MGA specs above for the actual Firebird-style record header fields (e.g., `rhd_transaction`, `rhd_back_version`) and visibility/GC rules.
  - L290: **Key Difference**: Use HLC for causal ordering, not XID (XIDs may be out-of-order across nodes).
  - L462: **Challenge**: Each node has local OIT/OAT, but back versions may be visible to transactions on other nodes.
  - L959: // Node B: Read user (should be visible)
  - L982: // GC attempt: Should NOT sweep back version (Node A's transaction still active)
  - L989: // GC attempt: Should sweep back version now
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/06_IMPLEMENTATION_PHASES.md
- Potential ambiguity markers detected:
  - L588: **Status**: SBCLUSTER-01, 02, 05 should be implemented before Phase 2 (Quorum).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/07_TESTING_STRATEGY.md
- Potential ambiguity markers detected:
  - L478: - **Jepsen** (optional): Distributed systems verification framework
  - L717: - Compare hashes (should be identical after convergence)
  - L756: ### Linearizability Verification (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/beta_requirements/replication/uuidv7-optimized/08_MIGRATION_OPERATIONS.md
- Potential ambiguity markers detected:
  - L591: read_quorum: 1  # Lower = faster reads, may be stale
  - L736: - Verify target nodes are online (hints should deliver)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/catalog/CATALOG_CORRECTION_PLAN.md
- Potential ambiguity markers detected:
  - L177: **Need Code Audit**: Many features marked "partial" may be complete
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/catalog/SCHEMA_PATH_RESOLUTION.md
- Potential ambiguity markers detected:
  - L57: - Names are user-facing; the executor should prefer UUIDs from the resolver.
  - L81: - Emulated parsers may use them internally for rewrites.
  - L89: - The emulating parser may use internal path tokens or `!:` during translation.
  - L101: - Some deployments may define special path keywords (e.g., `current`, `home`,
  - L104: - Optional security-gated behavior: relative paths (leading dot) may be
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md
- Potential ambiguity markers detected:
  - L410: uint32_t array_size;              // Fixed array size (0 = unspecified)
  - L527: // Shadow rebuild / versioning (current implementation)
  - L575: // Shadow rebuild / versioning (current implementation)
  - L591: Design target for shadow index rebuild + swap. The current implementation
  - L735: UuidV7Bytes owned_by_table_id;           // Optional owned-by table ID
  - L736: UuidV7Bytes owned_by_column_id;          // Optional owned-by column ID
  - L862: std::vector<std::string> column_names;  // Optional explicit columns
  - L2264: UuidV7Bytes signature_oid;           // Optional TOAST reference
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/catalog/UUID_LIFECYCLE_RULES.md
- Potential ambiguity markers detected:
  - L58: - IDs are not required to be unique across different databases, but SHOULD be
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/compression/COMPRESSION_FRAMEWORK.md
- Potential ambiguity markers detected:
  - L67: 1. Check if page should be compressed (type, size, settings)
  - L221: - LZ4 library (optional but recommended)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/CACHE_AND_BUFFER_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L74: | OS page cache (optional)     | Disk                           |
  - L163: ### 6.0 V3 Scope vs. Optional Extensions
  - L171: **Optional extensions (cluster-only):**
  - L179: 1. **Clock-sweep** eviction (retain current implementation).
  - L183: 5. **Optional multi-pool** layout (OLTP/OLAP/temp or per-tablespace pools).
  - L192: - Optional admission control (e.g., TinyLFU/2Q) to prevent one-hit pollution.
  - L193: - Optional compression for cached blocks.
  - L227: - **Statistics version** (optional, for plan cache)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/CORE_IMPLEMENTATION_SPECS_SUMMARY.md
- Potential ambiguity markers detected:
  - L39: - Implement all 28 core index types in phased delivery (no optional index types)
  - L159: V3 implementation MUST follow the authoritative specs above with no optional
  - L172: Each specification includes validation tests that should be implemented alongside the features:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/ENGINE_CORE_UNIFIED_SPEC.md
- Potential ambiguity markers detected:
  - L67: Write-after log (WAL) is optional post-gold only (replication/PITR).
  - L116: - Optional page compression and compressed page I/O.
  - L139: **Optional Alpha:** Materialized object-name registry for fast lookup
  - L140: (`/docs/specifications/parser/v3/alpha_requirements/optional/OBJECT_NAME_REGISTRY_MATERIALIZED_VIEW.md`).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/GIT_METADATA_INTEGRATION_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L32: - **Non-Invasive:** Optional feature, databases work without Git integration
  - L115: - **Tag**: optional named reference to a commit
  - L125: - Optional tags identify release points (`sbdb-1.0`, `sbdb-1.1`, etc).
  - L142: - Production environments should be Import-only or Export-only.
  - L1348: - Optional: GitHub/GitLab API for PR integration
  - L1368: - Some DDL operations may require exclusive locks
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/IMPLEMENTATION_RECOMMENDATIONS.md
- Potential ambiguity markers detected:
  - L275: uint64_t        page_lsn;        // For optional write-after log (WAL)
  - L445: ScratchBird should adopt a "best of breed" approach:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L156: std::optional<std::string> error_message;
  - L252: std::optional<std::string> block_reason;
  - L427: std::optional<std::string> where_clause; // Partial migration filter
  - L698: std::optional<Conflict> detectConflict(
  - L729: std::optional<Conflict> conflict;
  - L888: std::optional<std::string> error;
  - L1318: std::optional<std::string> row_key;
  - L1319: std::optional<std::string> source_error;
  - L1411: std::optional<std::vector<TypedValue>> local_values;
  - L1412: std::optional<std::vector<TypedValue>> remote_values;
  - L1437: std::optional<std::string> ssl_cert_path;
  - L1438: std::optional<std::string> ssl_key_path;
  - L1439: std::optional<std::string> kerberos_principal;
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/NAMESPACE_FUNCTION_MAP.md
- Potential ambiguity markers detected:
  - L3: This document consolidates source and specification references into a single lookup list for refactoring and validation. Line numbers are best-effort and may drift as files change.
  - L104: | `docs/specifications/alpha_requirements/optional/OBJECT_NAME_REGISTRY_MATERIALIZED_VIEW.md` | 14 |
  - L105: | `docs/specifications/alpha_requirements/optional/README.md` | 3 |
  - L238: | `docs/specifications/beta_requirements/optional/AUDIT_TEMPORAL_HISTORY_ARCHIVE.md` | 26 |
  - L239: | `docs/specifications/beta_requirements/optional/README.md` | 3 |
  - L240: | `docs/specifications/beta_requirements/optional/STORAGE_ENCODING_OPTIMIZATIONS.md` | 32 |
  - L241: | `docs/specifications/beta_requirements/optional/TABLESPACE_SHRINK_COMPACTION.md` | 13 |
  - L666: | `docs/specifications/parser/v3/beta_requirements/optional/AUDIT_TEMPORAL_HISTORY_ARCHIVE.md` | 26 |
  - L667: | `docs/specifications/parser/v3/beta_requirements/optional/README.md` | 3 |
  - L668: | `docs/specifications/parser/v3/beta_requirements/optional/STORAGE_ENCODING_OPTIMIZATIONS.md` | 32 |
  - L669: | `docs/specifications/parser/v3/beta_requirements/optional/TABLESPACE_SHRINK_COMPACTION.md` | 13 |
  - L790: | `docs/specifications/parser/v3/development/TODO.md` | 102 |
  - L1248: | heading | `### 4.5. Page Index (Optional)` | 450 |
  - L1323: | heading | `# Upload to S3 (optional)` | 2386 |
  - L1453: | heading | `### Phase 3: SQL/PGQ Graph Queries (Beta 3.0 - OPTIONAL)` | 2572 |
  - L1709: | heading | `### 6.4 Gossip-Based Observation (Optional Enhancement)` | 581 |
  - L2372: | heading | `#### SHOULD / RECOMMENDED` | 66 |
  - L2373: | heading | `#### SHOULD NOT / NOT RECOMMENDED` | 86 |
  - L2374: | heading | `### 2.3 Optional Features` | 103 |
  - L2375: | heading | `#### MAY / OPTIONAL` | 105 |
  - ... 421 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/THREAD_SAFETY.md
- Potential ambiguity markers detected:
  - L44: - Reads may proceed concurrently with other reads
  - L94: Commit sequence (no write-after log (WAL) in Alpha; optional post-gold only):
  - L99: Flush sequence (optional in Alpha):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/core/Y_VALVE_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L557: // Step 2: Detect protocol if unknown
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/02_DDL_STATEMENTS_OVERVIEW.md
- Potential ambiguity markers detected:
  - L14: * **IF \[NOT\] EXISTS Clause**: Most CREATE and DROP statements support the optional IF EXISTS or IF NOT EXISTS clause, which turns a potential error into a notice, simplifying scripting and automated deployments.
  - L50: | **DOMAIN** | A user-defined data type with optional constraints (NOT NULL, CHECK). ScratchBird extends this concept to include complex records, enums, sets, and embedded security rules. | [DDL\_DOMAINS\_COMPREHENSIVE.md](../types/DDL_DOMAINS_COMPREHENSIVE.md) |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/CASCADE_DROP_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L264: **Note:** When altering column from domain to base type, **domain constraints are lost** (CHECK, NOT NULL). User should recreate constraints if needed:
  - L483: -- ⚠️ Queries may slow down but still work
  - L907: - ✅ Should succeed
  - L908: - ✅ Should auto-drop owned objects (indexes, triggers)
  - L909: - ✅ Should clear dependencies
  - L915: - ❌ Should fail with error
  - L916: - ✅ Error should list object B
  - L917: - ✅ Object A should NOT be dropped (transaction rolled back)
  - L924: - ✅ Should succeed
  - L932: - ❌ Should fail with error
  - L933: - ✅ Error should list ALL dependents grouped by type
  - L940: - ✅ Table should be dropped
  - L941: - ✅ All indexes should be auto-dropped
  - L942: - ✅ All triggers should be auto-dropped
  - L948: - ❌ Should fail (schema not empty)
  - L951: - ✅ Should succeed (schema now empty)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_DATABASES.md
- Potential ambiguity markers detected:
  - L79: * ON SERVER: Optional server name. Defaults to `localhost`.
  - L81: * String literals may include OS paths or `server:/path` specs.
  - L83: * ALIAS: Optional list of alias names to create as synonyms under `emulated.<dialect>`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_EVENTS.md
- Potential ambiguity markers detected:
  - L42: - ScratchBird native clients may receive the event UUID.
  - L44: ### 2.3 Message Payload (optional)
  - L45: - ScratchBird can attach an optional message payload (text, up to 1024 bytes).
  - L85: - MESSAGE is optional and is ignored by Firebird protocol clients.
  - L126: ScratchBird native clients may receive:
  - L130: - Optional message payload
  - L146: - Optional config can restrict posting to specific roles or allowlisted prefixes.
  - L150: - Optional config can restrict which roles can register listeners.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_EXCEPTIONS.md
- Potential ambiguity markers detected:
  - L65: ScratchBird does not support ALTER EXCEPTION. To modify an exception, you must DROP and CREATE it again. This is by design to ensure that the contract for an exception (its name, parameters, and codes) remains stable for existing code that may handle it.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_ROLES_AND_GROUPS.md
- Potential ambiguity markers detected:
  - L203: | password_state | TEXT | ok / expired / must_change / unknown |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_ROW_LEVEL_SECURITY.md
- Potential ambiguity markers detected:
  - L24: This is essential for applications where different users or roles should see different subsets of data within the same table, such as in multi-tenant applications, or when sales representatives should only see their own customers.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_TABLES.md
- Potential ambiguity markers detected:
  - L35: * DEFAULT \<value\>: Provides a default value if one is not specified.
  - L41: ### **Storage Parameters (Optional)**
  - L43: Storage parameters configure table defaults and per-column overrides for varlen encoding, TOAST, and numeric storage. See `ScratchBird/docs/specifications/parser/v3/beta_requirements/optional/STORAGE_ENCODING_OPTIMIZATIONS.md`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_TABLE_PARTITIONING.md
- Potential ambiguity markers detected:
  - L132: otherwise the engine may scan multiple partitions.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_TEMPORAL_TABLES.md
- Potential ambiguity markers detected:
  - L44: * **HISTORY\_TABLE (Optional)**: You can optionally specify a name for the history table. If omitted, ScratchBird creates one with a default name.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_TRIGGERS.md
- Potential ambiguity markers detected:
  - L11: Triggers may declare and use cursor handles inside the trigger body, but they
  - L46: * INSTEAD OF: For views, specifies that the trigger should run *instead of* the DML operation.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/DDL_TYPES.md
- Potential ambiguity markers detected:
  - L23: reusable type and may be referenced by domains, table columns, and function
  - L127: - Optional `RECEIVE`/`SEND` define binary wire format conversions.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/EXTRACT_AND_ALTER_ELEMENT.md
- Potential ambiguity markers detected:
  - L15: This spec covers the EXTRACT and ALTER_ELEMENT expressions only. It does not define SQL dot notation, JSON operators, or dialect-specific function aliases, though it may reference them as synonyms.
  - L50: - Element names not defined for a type raise an error.
  - L83: | UNKNOWN | VALUE [R] |
  - L116: #### UNKNOWN
  - L117: - VALUE [R] -> UNKNOWN. (Reserved; not persisted.)
  - L296: - SRID [RW] -> INT32 (0 = undefined).
  - L425: - ALTER_ELEMENT should compile to a dedicated SBLR opcode with the element
  - L431: - For composite fields, unknown field names raise an error.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ddl/README.md
- Potential ambiguity markers detected:
  - L22: DDL statements define the structure of database objects including databases, schemas, tables, views, indexes, procedures, functions, and more. ScratchBird supports DDL from multiple SQL dialects (PostgreSQL, MySQL, Firebird; MSSQL optional extension) all mapped to a common internal representation.
  - L131: - **MSSQL** - SQL Server DDL syntax (IDENTITY, ON [PRIMARY], etc.) (optional extension)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/deployment/INSTALLATION_AND_BUILD_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L36: **Optional libraries (feature-gated)**
  - L139: - Bundled shared libs should be version-pinned and optionally checksummed on
  - L145: - Some optional dependencies are LGPL (e.g., GEOS/PROJ/libxml2). Static linking
  - L150: ScratchBird may ship with a sealed runtime bundle that includes all required
  - L160: - `lib/` bundled OpenSSL + zlib (and optional libs when enabled)
  - L235: - Optional libraries enable optional functionality; absence must not prevent
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/deployment/INSTALLER_FEATURES_AND_CONFIG_GENERATOR.md
- Potential ambiguity markers detected:
  - L17: - Beta: Optional components (emulation listeners, drivers, cluster features).
  - L29: Legend: R = Required, O = Optional, N/A = Not applicable
  - L41: | ODBC driver | O (`scratchbird-odbc`) | O (Feature: ODBCDriver) | OFF | Optional connectivity |
  - L42: | JDBC driver | O (`scratchbird-jdbc`) | O (Feature: JDBCDriver) | OFF | Optional connectivity |
  - L46: | Signed runtime bundle | O (`scratchbird-bundle`) | O (Feature: SignedRuntime) | OFF | Optional security mode |
  - L81: - All optional features must be selectable in UI and via command line
  - L154: - `/etc/scratchbird/security/` (optional)
  - L172: - Default tablespace layout (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/deployment/SYSTEMD_SERVICE_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L133: **Note:** TDS/MSSQL listener is reserved for optional extension; current versions expose Native/PG/MySQL/Firebird only.
  - L142: | Checkpoint | 1 per database | MGA checkpointing (no write-after log (WAL, optional extension)) |
  - L183: --tds-port <PORT>           TDS/MSSQL protocol port (optional extension, reserved)
  - L246: - TDS: 7.4 (optional extension)
  - L341: # TDS/MSSQL protocol port (reserved; optional extension, 0 to disable)
  - L490: # Optional write-after log (optional extension). MGA does not use write-after log (WAL) for recovery; these are no-ops until implemented.
  - L491: # Write-after log (WAL, optional extension) directory (default: same as data directory)
  - L500: # Maximum write-after log (WAL, optional extension) size before checkpoint (bytes)
  - L503: # Minimum write-after log (WAL, optional extension) size to retain
  - L515: # Write-after log (WAL, optional extension) compression
  - L602: # TLS certificate verification: none | optional | required
  - L769: # TODO: Rename autovacuum_* to gc_* after current work completes; keep autovacuum_* as
  - L814: # Write-after log (WAL, optional extension) sender processes
  - L1001: # Optional dependencies
  - L1076: ### 6.2 Socket Activation (Optional)
  - L1265: ├── Skip write-after log (WAL) replay (MGA); optional write-after log replay if configured
  - L1275: ├── TDS (1433, optional extension)
  - L1316: ├── Skip write-after log (WAL) replay (MGA); optional write-after log replay if configured
  - L1741: └── wal/                # Optional write-after log files (optional extension)
  - L1822: **Optional Dependencies:**
  - ... 2 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/deployment/WINDOWS_CROSS_COMPILE_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L17: - Toolchain: MinGW-w64 (primary), LLVM-mingw (optional).
  - L26: - `ninja` (optional, recommended)
  - L27: - `pkg-config` (optional, for host discovery)
  - L29: ### 3.2 LLVM-mingw (Optional)
  - L41: **Optional**
  - L70: - Optional libraries may remain dynamic if static builds are unavailable.
  - L74: Windows builds may ship as a sealed bundle with signed libraries.
  - L86: - Optional enterprise enforcement via WDAC/Code Integrity policies.
  - L134: - Optional: lz4, geos, proj, libxml2 DLLs
  - L155: - Run unit tests under Wine (optional).
  - L156: - CI should at minimum validate that binaries link and start.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/ARCHITECTURE_CLARIFICATION.md
- Potential ambiguity markers detected:
  - L152: The reorganized phases should emphasize:
  - L157: 5. **WAL optional**: System works without it (in-memory mode)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/ARCHITECTURE_GOALS.md
- Potential ambiguity markers detected:
  - L20: - See `/docs/specifications/parser/v3/thread_safety.md` for accurate current implementation details
  - L305: - ✅ Instead: Incremental, optional features
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/CLAUDE_DESIGN_PROPOSAL.md
- Potential ambiguity markers detected:
  - L75: - May need post-processing for context
  - L226: std::optional<TypeInfo> resolved_type;  // After semantic analysis
  - L227: std::optional<UUID> resolved_object_id; // For table/column references
  - L612: enum class TriBool { True, False, Unknown };
  - L679: - May need custom extensions
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/Design_Decisions_Report.md
- Potential ambiguity markers detected:
  - L10: ### Current Implementation Snapshot (Alpha 1.01.2)
  - L58: - Option A: Minimal BLR-compatible module now (Header + Code + Constants + Relation/Field descriptors + optional Debug lines).
  - L65: - Debug info: optional offset→line in debug builds only.
  - L89: - NULL: three-valued logic; WHERE treats UNKNOWN as false.
  - L95: - Recommendation: Proceed with golden tests; microbenchmarks optional later.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/GARBAGE_COLLECTION_DESIGN.md
- Potential ambiguity markers detected:
  - L31: Garbage collection (GC) is the process of reclaiming space occupied by old tuple versions that are no longer visible to any transaction. In an MVCC system like ScratchBird, UPDATE and DELETE operations create new tuple versions, leaving old versions in place for transactions that may still need them.
  - L92: - Cold data may accumulate garbage
  - L115: - May scan pages unnecessarily
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/ISOLATION_LEVELS_DESIGN.md
- Potential ambiguity markers detected:
  - L98: - May see inconsistent state across statements
  - L182: - May see "old" data if transaction runs long
  - L233: ### Current Implementation (Simple)
  - L380: // Fallback to READ COMMITTED (should not happen)
  - L468: - `src/core/connection_context.cpp` (table locking MUST be enabled as specified; TODO markers are invalid)
  - L635: // Connection 1: Should NOT see Alice (repeatable read)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/PAGE_SIZE_PERFORMANCE_CONSIDERATIONS.md
- Potential ambiguity markers detected:
  - L16: - **Cache inefficiency**: Filesystem cache may hold partial database pages, wasting memory
  - L20: - Testing on GitHub's infrastructure with unknown filesystem configuration
  - L26: With a dedicated large database cache and reader/writer threads, larger pages should show:
  - L73: - **64KB pages**: Should be only 10-20% slower per operation (not 400%+)
  - L74: - **128KB pages**: Should be 20-30% slower per operation (not 600%+)
  - L108: The current implementation is solid - the performance characteristics will be much better in a production environment with proper configuration. The flexibility to choose page sizes based on deployment environment is a key strength.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/SWEEP_MECHANISM_DESIGN.md
- Potential ambiguity markers detected:
  - L172: // 3. Optional: Remove old tuple versions (if foreground)
  - L214: **Optimization:** This is a simplified version. Production implementation should:
  - L231: // NOTE: This is expensive and should only be done in foreground sweep
  - L429: // OIT should have advanced
  - L449: // All transactions are committed, OIT should advance
  - L453: EXPECT_GE(oit, xids[99]); // OIT should be at least last committed XID
  - L474: - Should only be manual
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/TRANSACTION_MANAGEMENT_DESIGN.md
- Potential ambiguity markers detected:
  - L325: Some DDL operations may require being in their own transaction (no other work before or after):
  - L331: -- Implementation may enforce:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/alpha_1_05_design_synthesis.md
- Potential ambiguity markers detected:
  - L97: // Optional debug
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/alpha_1_05_outstanding_decisions.md
- Potential ambiguity markers detected:
  - L91: uint32_t debug_offset;    // Optional
  - L166: Value default_value;   // Optional
  - L189: - Optional components (e.g., debug tools)
  - L237: 1. **Performance**: Hand-written parser may be slower than generated
  - L243: 3. **Complexity**: Semantic analysis may grow complex
  - L274: While the high-level design is solid, these implementation details should be decided before coding begins. Most decisions are straightforward and follow established patterns from other databases. The proposed decisions above provide a concrete starting point that can be refined during implementation.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/alpha_1_05_sblr_examples.md
- Potential ambiguity markers detected:
  - L27: This document shows example SBLR bytecode generated for the SQL statements supported in Alpha 1.05. All examples below are **actually working** in the current implementation.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/materialized-views-foundation.md
- Potential ambiguity markers detected:
  - L137: // Parse and execute view definition (current implementation)
  - L192: Indexes: Optional (can add indexes for performance)
  - L248: - **Staleness**: Data may be outdated until refresh
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/design/thread_safety.md
- Potential ambiguity markers detected:
  - L37: - Only one Database instance should be opened per database file
  - L111: - Single process with multiple threads: undefined behavior
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/AI_CONTEXT_MEMORY_GUIDE.md
- Potential ambiguity markers detected:
  - L7: **Purpose**: This document defines what directories and files an AI assistant should keep in memory between context compactions to maintain effective continuity when working on the ScratchBird database engine.
  - L24: - **`/docs/development/TODO.md`** - Prioritized work items and blockers (50KB, updated Oct 13)
  - L62: ├── development/    # How to develop (TODO, standards, analysis)
  - L166: 3. `/docs/development/TODO.md` - Know what's prioritized
  - L180: - `/docs/development/TODO.md` (what's next)
  - L190: Oct 13: /docs/development/TODO.md
  - L210: ### "What should I work on next?"
  - L211: → Read `/docs/development/TODO.md` (prioritized by CRITICAL/HIGH/MEDIUM)
  - L280: 4. Check TODO.md for any related tasks
  - L380: 2. **TODO**: What's next (`/docs/development/TODO.md`)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/AI_PARALLEL_DEVELOPMENT_GUIDE.md
- Potential ambiguity markers detected:
  - L446: **Agent-generated code should**:
  - L452: **Human review should verify**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/BUILD_FIX_TODO_LIST.md
- Potential ambiguity markers detected:
  - L1: # Build System Fix - Actionable TODO List
  - L77: # Should return: no matches
  - L87: # Find where this constant should be defined:
  - L140: **Expected:** Header uses camelCase, implementation should match
  - L201: **Size:** Should be several MB
  - L301: **If broken:** Some auto-fixes may have introduced issues - review and fix
  - L379: **Note:** May not fix all 13,000+ warnings - focus on HIGH severity
  - L418: **Acceptable:** Some tests may reveal issues that were hidden by build errors
  - L463: **PR Description:** Link to this TODO list and results document
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/BUILD_INSTRUCTIONS.md
- Potential ambiguity markers detected:
  - L12: - **lz4:** The lz4 library is an optional dependency for compression support.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/CATALOG_DESIGN_REQUIREMENTS.md
- Potential ambiguity markers detected:
  - L13: This document captures the authoritative design requirements for ScratchBird's system catalog structure based on the project owner's specifications. These requirements override any current implementation that conflicts with them.
  - L31: // WRONG (current implementation)
  - L142: **TOAST is fully implemented. All `*_oid` fields should be activated.**
  - L455: - `search_path_oid` should NOT be in SchemaRecord
  - L518: uint32_t search_path_oid;     // ❌ Should not exist
  - L572: This document represents the authoritative design requirements from the project owner. Any current implementation that conflicts with these requirements MUST be corrected.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/CODING_STANDARDS.md
- Potential ambiguity markers detected:
  - L48: - See [TODO.md MED-001](TODO.md#med-001-standardize-naming-conventions) - **COMPLETED**
  - L74: // TODO: comment (42+ instances)
  - L75: // TODO(category): comment (some instances)
  - L86: - Convert TODOs to GitHub issues (see TODO.md LOW-001)
  - L94: The project uses a `Status` and `ErrorContext` based error handling mechanism. Exceptions should not be used for control flow.
  - L96: - All functions that can fail should return a `Status` enum.
  - L97: - For functions that need to return a value, the value should be returned via an output parameter or use trailing return type with Status.
  - L98: - The `ErrorContext` struct should be used to provide detailed error information, including the file, line number, and a descriptive error message.
  - L99: - The `SET_ERROR_CONTEXT` macro should be used to set the error context.
  - L117: **Pattern 3:** Optional ErrorContext parameter
  - L133: **CRITICAL:** Error handling standardization required. See [TODO.md CRIT-003](TODO.md#crit-003-standardize-error-handling-pattern).
  - L137: **DECISION: Option B - ErrorContext Always Optional**
  - L150: 2. **Function signatures: Default to optional**
  - L152: // Public API - always optional
  - L171: * @param ctx Error context (optional, can be nullptr)
  - L176: **Optional Helper Macros (can be added later):**
  - L228: **Recommendation:** Convert lock_manager to use smart pointers. See [TODO.md HIGH-005](TODO.md#high-005-convert-lock-manager-to-raii).
  - L242: - Use `[[nodiscard]]` for functions whose return value should not be ignored.
  - L268: uint32_t last_page = 100;            // Should be named constant
  - L269: header->max_connections = 1;         // Should be config
  - ... 24 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/DOCUMENTATION_CORRECTIONS_SUMMARY.md
- Potential ambiguity markers detected:
  - L99: **Impact:** Developers understand this is aspirational architecture, not current implementation. Critical for setting realistic expectations.
  - L123: **Current Implementation:**
  - L274: **This report should be considered HISTORICAL ONLY until the build is fixed.**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/DOCUMENTATION_REORGANIZATION_PLAN.md
- Potential ambiguity markers detected:
  - L23: The following STATUS_* and phase-specific files are in `/docs/` root but should be in `/docs/specifications/parser/v3/status/`:
  - L52: These summary/completion files should be in `/docs/specifications/parser/v3/status/`:
  - L64: These should be in `/docs/archive/2026-01-09/planning/` or `/docs/specifications/parser/v3/audit/`:
  - L81: The following guide files should be consolidated in `/docs/specifications/parser/v3/guides/`:
  - L85: CI_CD_GUIDE.md → Already should be here (OK)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/INDEX_FACTORY_FIX_NOTES.md
- Potential ambiguity markers detected:
  - L21: 3. Add TODO comments for future proper integration with index-specific parameters
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/PROCESS_AND_AGENTS.md
- Potential ambiguity markers detected:
  - L9: When an AI session is started, it will be given its role and the current task. The agent should use the following documents to understand its duties, the project's status, and the rules it needs to follow.
  - L21: - Test Writer (Agent C): Authors performance/hardening/edge-case tests. Core validation for Agent A. May mark tests as future if feature belongs to later stages.
  - L35: - File names should be based on the task and date (e.g., `agent_b_code_review_2025-09-08.md`).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/TEST_COMPLIANCE_IMPLEMENTATION_SUMMARY.md
- Potential ambiguity markers detected:
  - L167: - Should be added to `test_mga_integration.cpp`
  - L172: - Should be added to `test_mga_integration.cpp`
  - L211: - Issue: Large tuple insert succeeds when it should fail
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/TODO.md
- Potential ambiguity markers detected:
  - L1: # ScratchBird Development TODO
  - L292: - ✅ All 15+ locking TODO markers resolved
  - L406: - ⚠️ Note: Key extraction currently uses simplified tuple parsing (TODO at line 1006-1009 for future enhancement)
  - L529: - Investigation revealed TODO.md had incorrect description (claimed "TIP Page Logic")
  - L970: - Create RAII wrapper for pinned pages (optional, template provided)
  - L1024: "Correcting to buffer size (this may indicate corruption or config change).",
  - L1050: ### LOW-001: Convert TODO Markers to GitHub Issues
  - L1056: 1. Create GitHub issues for each TODO
  - L1057: 2. Add issue numbers to TODO comments
  - L1263: **Current State:** TODO marker
  - L1433: **Current TODO Items:**
  - L1471: **Note:** This TODO list is based on actual code analysis, not on documentation or comments. All issues are verified to exist in the codebase as of October 11, 2025.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/Test Suite Specification.md
- Potential ambiguity markers detected:
  - L90: - Test updating a row from a TOASTed value to a small value (should clean up TOAST chunks).
  - L102: - **Hash Index:** The existing 12-test suite is sufficient and should be validated to pass post-MGA.
  - L184: - **Lexer/Parser/Analyzer:** The existing tests are comprehensive. They should be organized, enabled, and run as a single suite. No major new tests are required for Alpha.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/development/UUID_ARCHITECTURE_AUDIT_AND_FIXES.md
- Potential ambiguity markers detected:
  - L329: ### Optional Improvements (Not Critical):
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/dml/DML_COPY.md
- Potential ambiguity markers detected:
  - L84: - `MAX_ERRORS` and `ON_ERROR` are optional safety valves; if `MAX_ERRORS=0`, any error aborts.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/dml/DML_INSERT.md
- Potential ambiguity markers detected:
  - L27: Inserts exactly one row into a table. The column list is optional if values are provided for all columns in their defined order.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/dml/DML_MERGE.md
- Potential ambiguity markers detected:
  - L74: This example extends the upsert to also handle records that exist in the target but not in the source (e.g., they should be deleted or marked inactive).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/dml/DML_XML_JSON_TABLES.md
- Potential ambiguity markers detected:
  - L34: * **path\_to\_array**: A JSONPath expression that points to the array within the document that should be iterated over. Each element in this array will become a row in the output table. Use '$' for a single object.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/findings/NO_GREY_AREAS_GATE.md
- Potential ambiguity markers detected:
  - L48: - `CLOSED` Formal bytecode canonicalization verifier (ordering, optional field constraints) in `SBLR_V3_VALIDATION_RULES.md` + `SBLR_V3_BYTECODE_CANONICALIZATION.md`.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/guides/ERROR_HANDLING_GUIDE.md
- Potential ambiguity markers detected:
  - L154: | 1200-1299 | Database errors | Check db, may need recovery |
  - L260: 3. **Basic guarantee**: No leaks, invariants maintained, but state may change
  - L265: 4. **No guarantee**: May leak or corrupt (avoid!)
  - L275: data.resize(large_size);  // May throw std::bad_alloc
  - L287: std::string path = base_path + "/" + filename;  // May throw
  - L299: map.insert(std::make_pair(key, value));  // May throw
  - L569: // BAD: Exceptions should only be std::bad_alloc
  - L709: // Try to allocate one more - should fail
  - L758: EXPECT_EQ(s, Status::KEY_NOT_FOUND);  // Should not exist
  - L917: // Insert into leaf (may split)
  - L933: // Split failed - index may be corrupted
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/guides/PHASE_1_5_COMPLETION_GUIDE.md
- Potential ambiguity markers detected:
  - L264: **Note**: HNSW nodes store TIDs - may need on-disk format update.
  - L420: // TableScan::next() should return TID:
  - L762: // 6. MVCC filtering (optional - usually done at storage layer)
  - L972: # Should be minimal - only at API boundaries
  - L976: # Should return 0 results (all should be TID)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/guides/PHASE_1_5_FINAL_STEPS.md
- Potential ambiguity markers detected:
  - L241: 2. **Undefined reference**: Missing method signature updates
  - L361: # Should have minimal results - only at API boundaries
  - L367: # Should return 0 - no uint64_t tuple_id in headers
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/guides/RESOURCE_MANAGEMENT.md
- Potential ambiguity markers detected:
  - L52: **Unpin**: Release reference (page may be evicted)
  - L88: // Work with page - may fail
  - L615: // Should be able to pin again
  - L648: // Page should still be unpinned properly (via RAII guard)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/AdaptiveRadixTreeIndex.md
- Potential ambiguity markers detected:
  - L65: - `version_chain`: optional node versioning for concurrent writers (RCU-style).
  - L233: - Snapshot pages should be aligned to 8K+ for efficient scan
  - L244: - Leaf entries must use `record_uuid` with optional `SBRecordPtr` cache hints.
  - L350: - optional write-after log for faster recovery
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/AdvancedIndexes.md
- Potential ambiguity markers detected:
  - L55: positions: [[u32]]            # Word positions (optional)
  - L68: • Stop word removal (optional)
  - L513: std::optional<DataType> min_value_;
  - L514: std::optional<DataType> max_value_;
  - L517: void Update(const std::optional<DataType>& value) {
  - L754: - Training vectors should represent dataset distribution
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/BITMAP_SPEC.md
- Potential ambiguity markers detected:
  - L81: - Bitmap may return false positives; executor must filter.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/BloomFilterIndex.md
- Potential ambiguity markers detected:
  - L67: - **Counting Bloom Filter (optional):** maintain 4/8-bit counters and decrement on delete.
  - L81: `record_uuid` with optional `SBRecordPtr` cache hints. Legacy TID encodings are not permitted.
  - L153: - Planner may apply bloom before exact index or heap scan when selectivity is low.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/COLUMNSTORE_SPEC.md
- Potential ambiguity markers detected:
  - L73: use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy TID encodings are not permitted.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/CountMinSketchIndex.md
- Potential ambiguity markers detected:
  - L85: ### Conservative Update (optional)
  - L100: - CMS is **auxiliary** and may be stale after rollbacks.
  - L117: Implement CMS as an **auxiliary index** attached to a column. It stores a hash-based counter matrix. CMS is not exact; it should be used for approximate queries or planner hints.
  - L206: - DELETE: optional decrement if negative counters enabled; otherwise ignore and rebuild periodically
  - L224: optional accuracy controls:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/FSTIndex.md
- Potential ambiguity markers detected:
  - L186: - Segment files should target 16-64MB for good cache behavior
  - L233: Posting lists must store `record_uuid` with optional `SBRecordPtr` cache hints.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/GIN_SPEC.md
- Potential ambiguity markers detected:
  - L72: - Posting trees may be compacted/rebuilt when sparse.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/GIST_SPEC.md
- Potential ambiguity markers detected:
  - L60: - Merge nodes if underfull (optional optimization; correctness invariant).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/GeohashS2Index.md
- Potential ambiguity markers detected:
  - L239: Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
  - L307: - optional ART backend for in-memory geohash
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/HNSW_SPEC.md
- Potential ambiguity markers detected:
  - L67: - Rewire neighbors during GC compaction (optional optimization; correctness invariant).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/HyperLogLogIndex.md
- Potential ambiguity markers detected:
  - L99: - HLL is **auxiliary** and may be stale after rollbacks.
  - L205: - UPDATE: add new value; optional full rebuild for accuracy
  - L220: optional accuracy controls:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L50: or index semantics. It may only affect performance in future versions.
  - L217: - Hash collisions may degrade performance
  - L265: - ⚠️ Posting list entries use physical removal (should use MGA versioned delete)
  - L425: - Lossy: May return false positives (requires heap verification)
  - L658: - Fuzzy/prefix dictionary integration via FST (optional)
  - L707: - `back_version_uuid` (UUID): optional: rhd_back_version for validation
  - L711: - `record_uuid` is authoritative. `record_ptr` may be stale and must be validated.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_GUIDE.md
- Potential ambiguity markers detected:
  - L37: - `back_version_uuid` (UUID): optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md
- Potential ambiguity markers detected:
  - L43: - `back_version_uuid` (UUID): optional
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/IVFIndex.md
- Potential ambiguity markers detected:
  - L76: 3. (optional) Train **product quantization (PQ)** codebooks on residual vectors.
  - L187: **optional:** Integrate Faiss library (if unavailable, use internal implementation)
  - L201: - **GPU support:** optional GPU acceleration for training/search
  - L235: 5. Re-rank Top K: (optional) with full vectors
  - L294: ### 2. BLAS/LAPACK (optional but Recommended)
  - L902: - **Popular clusters:** May contain 10x average vectors
  - L903: - **Sparse clusters:** May contain very few vectors
  - L909: - Some lists may have 5K+ vectors (hot spots)
  - L1266: - **Row identity:** Store row references as `record_uuid` with optional `SBRecordPtr` cache hints.
  - L1268: - **Heap fetch:** Result resolution must resolve `record_uuid` to a record header (pointer may be stale).
  - L1358: // Reason: Vector may move to different cluster
  - L1653: ### Faiss Integration (optional)
  - L1658: - [ ] Add optional GPU support flags
  - L1744: - [ ] Multithreading for search (optional)
  - L1883: EXPECT_EQ(results.size(), 0);  // Should not see uncommitted inserts
  - L1891: EXPECT_GT(results.size(), 0);  // Should see committed inserts
  - L1936: EXPECT_LT(duration.count(), 300);  // Should complete within 5 minutes
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/InvertedIndex.md
- Potential ambiguity markers detected:
  - L77: - optional stopword removal and stemming/lemmatization.
  - L91: 3. Use FST or B‑tree for dictionary lookup (optional).
  - L251: ### 2. ICU (International Components for Unicode) - optional
  - L256: **ICU (optional):** If ICU is available, use it; otherwise fall back to built-in UCA tables.
  - L278: │   └── (optional) Word positions
  - L562: - `positions` (std::vector<std::vector<uint32_t>>): (optional) Word positions
  - L571: Positions (optional): [[3,10], [7], [2,8,15], [12], [1,5,9,13,18]]
  - L582: [(optional) Compressed positions]
  - L604: • Stop word removal (optional)
  - L811: // 2. For each term, get posting list (may include uncommitted docs)
  - L874: - **Row identity:** Posting lists store `record_uuid` with optional `SBRecordPtr` cache hints (no legacy page IDs).
  - L877: - **Visibility checks:** Record fetch uses `record_uuid` resolution; cached `SBRecordPtr` may be stale.
  - L1940: // Document 2 should rank higher (more occurrences)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/JSONPathIndex.md
- Potential ambiguity markers detected:
  - L156: - optional helper table (required): `sys.index_path_defs`
  - L178: The index should accelerate:
  - L185: When the predicate uses non-indexed paths, the planner should fall back to
  - L196: Posting lists must store `record_uuid` with optional `SBRecordPtr` cache hints.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/LSMTimeSeriesIndex.md
- Potential ambiguity markers detected:
  - L132: - optional per-row TTL from column
  - L150: - `lt_pk_column_id` (uint16_t): optional key
  - L205: - DELETE: optional tombstone with short retention
  - L213: Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/LearnedIndex.md
- Potential ambiguity markers detected:
  - L224: Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/QuadtreeOctreeIndex.md
- Potential ambiguity markers detected:
  - L231: Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
  - L243: - No movement of live entries; only removal and optional node merge.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/SuffixIndex.md
- Potential ambiguity markers detected:
  - L105: Implement **suffix arrays** as the primary structure, with an optional suffix tree mode for in-memory deployments.
  - L222: Record locators must use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are not
  - L282: - optional suffix tree for in-memory deployments
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/ZOrderIndex.md
- Potential ambiguity markers detected:
  - L257: Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
  - L263: - Leaf-level cleanup may trigger page merge/rebalance following the
  - L345: - optional KNN search using iterative range expansion
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/indexes/ZoneMapsIndex.md
- Potential ambiguity markers detected:
  - L21: Zone maps store min/max (and optional extra stats) for data **segments** to allow fast pruning of scans. Zone maps are **auxiliary** and never used for correctness without validating rows via MGA visibility.
  - L40: - optional: `bloom` or `distinct_estimate`
  - L60: - On DELETE: no direct min/max update; segment may become stale.
  - L79: If any zone map stores row references (optional), it must use `record_uuid` with
  - L80: optional `SBRecordPtr` cache hints. Legacy TID encodings are not permitted.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md
- Potential ambiguity markers detected:
  - L41: - `0xFF` Unknown/unsupported
  - L62: - Unknown message types MUST respond with `ERROR` and be ignored.
  - L79: - `reason_len:u16` + reason bytes (optional)
  - L130: Worker -> Listener. Periodic stats (optional JSON).
  - L172: - Unknown message types MUST be ignored with ERROR response.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/network/DIALECT_AUTH_MAPPING_SPEC.md
- Potential ambiguity markers detected:
  - L110: - `role_id` (SBDB$KEY_ROLE; optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md
- Potential ambiguity markers detected:
  - L66: - tls_info (optional)
  - L112: - Reject unknown opcodes.
  - L172: The fixed 32-byte header remains unchanged. v1.1 introduces an **optional**
  - L261: 2. Engine replies STARTUP_RESPONSE and MAY send FEATURE_NEGOTIATE.
  - L436: - `credits`: Number of chunks the sender may send. Positive values grant permission,
  - L588: | 0x0045 | DESCRIBE_RESULT | PARAMETER_DESCRIPTION / ROW_DESCRIPTION | 0x50 / 0x44 | IPC may return one or both. |
  - L916: - IPC status_code != 0 should emit SBWP ERROR before STREAM_END.
  - L917: - total_rows/total_bytes set to 0 if unknown.
  - L992: IPC payloads MUST be set to zero (or empty) to avoid undefined behavior.
  - L1000: - **STREAM_READY**: `total_rows = 0`, `estimated_bytes = 0` if unknown.
  - L1001: - **STREAM_DATA**: `chunk_rows = 0` if unknown.
  - L1002: - **STREAM_END**: `total_rows = 0`, `total_bytes = 0` if unknown.
  - L1609: tests and packet decoders. Parsers should reject payloads that do not match the
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/network/NETWORK_LAYER_SPEC.md
- Potential ambiguity markers detected:
  - L46: - TLS is optional for ScratchBird native and required by configuration for
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md
- Potential ambiguity markers detected:
  - L120: optional: Shared listener (single port + protocol auto-detect).
  - L234: mode: required            # disabled | optional | required
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/network/PARSER_AGENT_SPEC.md
- Potential ambiguity markers detected:
  - L65: optional:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md
- Potential ambiguity markers detected:
  - L35: database rows (if `scope=engine` only, values may be reused for all databases).
  - L61: | usesysid | NULL | User/role OID mapping is not defined in V3; MUST return NULL |
  - L71: | wait_event_type | CASE wait_event != NULL -> 'Lock' | Optional classification |
  - L78: Columns not listed above should return NULL or documented PostgreSQL defaults.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md
- Potential ambiguity markers detected:
  - L32: views; no extra rows may be exposed.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md
- Potential ambiguity markers detected:
  - L32: 4. OID `0` means "unknown" and MUST NOT be assigned.
  - L67: - Emulated queries may request OID mapping on-demand; map lazily on first use.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/parser/08_PARSER_AND_DEVELOPER_EXPERIENCE.md
- Potential ambiguity markers detected:
  - L15: * **Be Forgiving and Explicit**: The parser should understand common developer patterns and dialect variations. When it cannot, it must provide clear, precise, and actionable error messages.
  - L16: * **Automate Documentation**: The system should leverage information already present in SQL scripts (like comments) to automatically generate and maintain schema documentation.
  - L18: * **Provide Insight**: Integrated tools should make it easy to understand query performance and database behavior without leaving the development environment.
  - L135: Hint: You may be missing a comma after the previous column definition 'name VARCHAR(100)'.
  - L153: 1\. Line 2, column 16: Unknown keyword 'PRIMRY'. Did you mean 'PRIMARY'?
  - L154: 2\. Line 3, column 10: Unknown data type 'VARCHR'. Did you mean 'VARCHAR'?
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/parser/PSQL_CURSOR_HANDLES.md
- Potential ambiguity markers detected:
  - L24: controls who may close the cursor by default.
  - L76: - `IN`: the callee may FETCH and move position, but must not CLOSE.
  - L77: - `IN OUT`: the callee may FETCH and CLOSE; position changes are visible
  - L88: - A handle may not be stored in tables, serialized, or passed to other
  - L105: - Triggers may declare and use cursor handles inside the trigger body.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/parser/README.md
- Potential ambiguity markers detected:
  - L22: ScratchBird implements a unique multi-dialect SQL parser that supports native ScratchBird SQL, PostgreSQL, MySQL, and Firebird dialects (MSSQL optional extension). This directory contains the complete grammar specifications, parser implementation details, and emulation layer designs.
  - L60: ### V3 Parser Consolidation (Implementation-First)
  - L90: 2. **Emulated Parsers** - PostgreSQL, MySQL, Firebird parsers that generate SBLR bytecode directly (MSSQL optional extension)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md
- Potential ambiguity markers detected:
  - L17: [ ]          optional (0 or 1)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/EXTERNAL_AGENTS_API_AUDIT_AND_REMEDIATION_PLAN.md
- Potential ambiguity markers detected:
  - L116: **Analysis:** The MySQL parser agent references a `PreparedStatement` type that should be defined locally or included from a header. This is likely a local struct for tracking prepared statement state.
  - L168: // Current definition may be missing these fields
  - L272: // Current definition may be:
  - L330: **Analysis:** The SBLR bytecode system may not have implemented transaction opcodes yet, or they have different names.
  - L356: **Estimated Effort:** 2-4 hours (may require SBLR changes)
  - L446: **Problem:** SBLR may not have transaction opcodes implemented.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/EXTERNAL_AGENTS_QUICK_FIX_GUIDE.md
- Potential ambiguity markers detected:
  - L164: // Should be:
  - L219: // TODO: Implement actual transaction bytecode when opcodes available
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/IPC_ENGINE_CHANGES_SUMMARY.md
- Potential ambiguity markers detected:
  - L76: - **Rationale:** Safest assumption; avoids undefined concurrency
  - L132: - **Long-term:** Optional migration to windowed if needed
  - L212: - Rationale: Matches current implementation
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/IPC_ENGINE_QUICK_FIX_REFERENCE.md
- Potential ambiguity markers detected:
  - L422: # Should compile without "No rule to make target" errors
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/IPC_ENGINE_REMEDIATION_PLAN_2026-02-06.md
- Potential ambiguity markers detected:
  - L861: - **Rationale:** Safest assumption; avoids undefined concurrency on socket I/O
  - L865: - **Decision (Long-term):** Optional migration to windowed model if team prefers
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/IPC_ENGINE_VERIFICATION_SUMMARY_2026-02-06.md
- Potential ambiguity markers detected:
  - L197: // TODO: Implement IPC integration
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/PLAN_ALPHA_COMPLETION.md
- Potential ambiguity markers detected:
  - L98: const std::vector<std::optional<std::string>>& values) override;
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/TABLESPACE_IMPLEMENTATION_PLAN.md
- Potential ambiguity markers detected:
  - L44: - 🔮 Task 5.4: ONLINE Migration (40-60 hours) - DEFERRED TO optional extension
  - L59: 7. [Future Phases](#future-phases-optional extension)
  - L366: - **FSM loading deferred to Task 1.3.5** (noted in TODO comment)
  - L375: - **FSM flushing deferred to Task 1.3.5** (noted in TODO comment)
  - L705: - ✅ TODO: Delete file from filesystem (deferred)
  - L706: - ✅ TODO: Invalidate catalog record (deferred to compaction)
  - L771: - [x] Can create tablespace with all optional parameters ✅ COMPLETE
  - L815: - ✅ TODO: Update TablespaceHeader on disk (requires PageManager API)
  - L821: - ✅ TODO: Update TablespaceHeader.tablespace_name on disk (requires PageManager API)
  - L876: - [x] TODO: Changes persisted to TablespaceHeader (deferred - requires PageManager API)
  - L905: - Updated `CreateTableStmt` AST node with optional tablespace field (StringPool::StringId)
  - L906: - Added optional TABLESPACE parsing in `Parser::parseCreateTable()`
  - L1012: **Assignee**: TBD
  - L1186: - Performance test: Preallocate 10GB, measure time (should be < 1 second with fallocate)
  - L1220: **Note**: Online migration (ONLINE clause) deferred to Phase 5 (optional extension).
  - L1228: **Assignee**: TBD
  - L1235: - [OFFLINE_TABLE_MIGRATION_TODOS.md](./OFFLINE_TABLE_MIGRATION_TODOS.md) - Detailed session-by-session todo lists
  - L1284: - ✅ Updated `moveTableToTablespace()` signature with optional progress_callback parameter
  - L1372: - Stress test: Migrate table under concurrent read load (should block readers)
  - L1380: **Assignee**: TBD
  - ... 14 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_DOTNET.md
- Potential ambiguity markers detected:
  - L41: - Smoke with Entity Framework Core provider (optional).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_FOUNDATION_LIBSCRATCHBIRD.md
- Potential ambiguity markers detected:
  - L20: - Native SBWP over TCP and optional Unix sockets only.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_NODEJS_TYPESCRIPT.md
- Potential ambiguity markers detected:
  - L39: - Smoke tests with Prisma/TypeORM optional adapters.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_PHP.md
- Potential ambiguity markers detected:
  - L15: - Deliver PDO driver (and optional mysqli compatibility layer) using SBWP.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_PYTHON.md
- Potential ambiguity markers detected:
  - L31: - Optional Pandas and NumPy integration hooks.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/archive/PLAN_DRIVER_RUST.md
- Potential ambiguity markers detected:
  - L22: - Core client crate and optional tokio feature.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/completed/CATALOG_EXPANSION_PLAN_finished.md
- Potential ambiguity markers detected:
  - L24: ## Current implementation summary (code truth)
  - L91: - Confirm whether index versioning should remain embedded in IndexRecord
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/completed/PLAN_DRIVER_SERVER_FEATURES_ALPHA_finished.md
- Potential ambiguity markers detected:
  - L15: item in that document as **required**, even if previously marked optional.
  - L114: 3. Add optional conformance test gated by env var.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/completed/RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN_finished.md
- Potential ambiguity markers detected:
  - L162: - High volume of collations (MySQL/PG) may be better served by ICU/OS locale
  - L164: - Charset aliasing may introduce ambiguity; enforce canonical name + alias mapping.
  - L167: - Engine/Resources: TBD
  - L168: - Tools: TBD
  - L169: - Documentation: TBD
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/completed/TRACKER_FIREBIRD_PARSER_ALPHA.md
- Potential ambiguity markers detected:
  - L24: ## optional extension
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/completed/TRACKER_INDEX_SPEC_GAPS_UPDATE_2026-01-22_finished.md
- Potential ambiguity markers detected:
  - L19: - [x] **V2 FULLTEXT parser gap:** add `USING FULLTEXT` and optional `USING INVERTED` alias (see `/docs/specifications/parser/v3/V2_PARSER_INDEX_TYPE_COMPLETENESS.md`).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/completed/TRACKER_MYSQL_PARSER_ALPHA.md
- Potential ambiguity markers detected:
  - L26: ## optional extension
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/planning/completed/TRACKER_OUTSTANDINGWORK_VERIFIED_2026-01-28.md
- Potential ambiguity markers detected:
  - L44: executor applies DISTINCT ON with optional ORDER BY sequencing via
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/query/PARALLEL_EXECUTION_ARCHITECTURE.md
- Potential ambiguity markers detected:
  - L34: - DOP MAY be reduced at runtime if resources are constrained.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/UUIDv7 Replication System Design.md
- Potential ambiguity markers detected:
  - L121: **Conclusion on Identifier Design:** The "Technical Debt" to be avoided is the overhead of random I/O and continuous Merkle tree re-computation. Therefore, the system **MUST use the UUIDv7 time component** for verification and object identification. The Hash Index type should be reserved only for secondary lookups where random access is explicitly required by the user query pattern, not for the internal replication engine.10
  - L131: In a "No Technical Debt" architecture, the verification structures should mirror the storage structures to minimize impedance mismatch. Modern LSM storage engines utilize **Time-Windowed Compaction Strategy (TWCS)**, where SSTables (data files) are bucketed by time.10
  - L133: The Anti-Entropy system should maintain a one-to-one mapping between these Storage Time Windows and the Verification Merkle Trees.
  - L230: The system should support a CONFLICT STRATEGY configuration per table:
  - L247: The robust solution is **Data Colocation** (Affinity). The system should allow the user to define a **Partition Key** that is shared across related tables.15
  - L261: For transactions that *must* span partitions (e.g., transferring money from User A to User B), the system should implement the **Saga Pattern**.
  - L276: * **Storage:** The catalog data itself should be stored using the same UUIDv7-based storage engine but replicated via **Raft** instead of Gossip.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/FirebirdReferenceDocument.md
- Potential ambiguity markers detected:
  - L810: *   Double quotes may be used as an alternative to apostrophes for delimiting string data. This is contrary to the SQL standard — double quotes are reserved for a distinct syntactic purpose both in standard SQL and in Dialect 3. Double-quoting strings is therefore to be avoided.
  - L843: Use of Dialect 3 is strongly recommended for newly developed databases and applications. Both database and connection dialects should match, except under migration conditions with Dialect 2.
  - L853: The primary construct in SQL is the _statement_. A statement defines what the database management system should do with a particular data or metadata object. More complex statements contain simpler constructs — _clauses_ and _options_.
  - L857: A clause defines a certain type of directive in a statement. For instance, the `WHERE` clause in a `SELECT` statement and in other data manipulation statements (e.g. `UPDATE`, `DELETE`) specifies criteria for searching one or more tables for the rows that are to be selected, updated or deleted. The `ORDER BY` clause specifies how the output data — result set — should be sorted.
  - L865: All words that are included in the SQL lexicon are keywords. Some keywords are _reserved_, meaning their usage as identifiers for database objects, parameter names or variables is prohibited in some or all contexts. Non-reserved keywords can be used as identifiers, although this is not recommended. From time to time, non-reserved keywords may become reserved, or new (reserved or non-reserved) keywords may be added when new language feature are introduced. Although unlikely, reserved words may also change to non-reserved keywords, or keywords may be removed entirely, for example when parser rules change.
  - L889: *   The name must start with an unaccented, 7-bit ASCII alphabetic character. It may be followed by other 7-bit ASCII letters, digits, underscores or dollar signs. No other characters, including spaces, are valid. The name is case-insensitive, meaning it can be declared and used in either upper or lower case. Thus, from the system’s point of view, the following names are the same:
  - L922: *   It may contain any character from the `UTF8` character set, including accented characters, spaces and special characters
  - L951: boolean        - true, false, unknown
  - L963: Some of these characters, alone or in combination, may be used as operators (arithmetical, string, logical), as SQL command separators, to quote identifiers, or to mark the limits of string literals or comments.
  - L987: Comments may be present in SQL scripts, SQL statements and PSQL modules. A comment can be any text, usually used to document how particular parts of the code work. The parser ignores the text of comments.
  - L1001: Block comments start with the `/*` character pair and end with the `*/` character pair. Text in block comments may be of any length and can occupy multiple lines.
  - L1056: A fixed-length binary data type; synonym for `CHAR(_n_) CHARACTER SET OCTETS` Values shorter than the declared length are padded with NUL (0x00) up to the declared length. If the number of characters is not specified, 1 is used by default.
  - L1064: A data type of variable size for storing large amounts of data, such as images, text, digital sounds. The blob subtype defines its content. Depending on the page size, `BLOB`s can exceed 4 GB, but some built-in functions and features may not be able to access data beyond 4 GB.
  - L1070: false, true, unknown
  - L1080: A fixed-length character data type. Values shorter than the declared length are padded with spaces (0x20) — or NUL (0x00) for character set OCTETS — up to the declared length. If the number of characters is not specified, 1 is used by default.
  - L1096: Decimal floating-point type, IEEE-754 _decimal64_ or _decimal128_. If the precision is not specified, 34 is used by default.
  - L1254: `INT128` is a 128-bit integer data type. This type is not defined in the SQL standard.
  - L1297: For testing data in columns with floating-point data types, expressions should check using a range, for instance, `BETWEEN`, rather than searching for exact matches.
  - L1366: These non-standard type names are deprecated and may be removed in a future Firebird version.
  - L1517: The form of declaration for fixed-point data, for instance, `NUMERIC(p, s)`, is common to both types. The `s` argument in this template is _scale_. Understanding the mechanism for storing and retrieving fixed-point data should help to visualise why: for storage, the number is multiplied by 10s (10 to the power of `s`), converting it to an integer; when read, the integer is converted back by multiplying by 10\-s (or, dividing by 10s).
  - ... 569 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/02_SQL_Language_Structure.md
- Potential ambiguity markers detected:
  - L89: ◦ Double quotes may be used as an alternative to apostrophes for delimiting string data. This
  - L118: applications. Both database and connection dialects should match, except under
  - L131: system should do with a particular data or metadata object. More complex statements contain
  - L137: ORDER BY clause specifies how the output data — result set — should be sorted.
  - L148: this is not recommended. From time to time, non-reserved keywords may become reserved, or
  - L149: new (reserved or non-reserved) keywords may be added when new language feature are
  - L150: introduced. Although unlikely, reserved words may also change to non-reserved keywords, or
  - L151: keywords may be removed entirely, for example when parser rules change.
  - L176: • The name must start with an unaccented, 7-bit ASCII alphabetic character. It may be followed
  - L205: • It may contain any character from the UTF8 character set, including accented characters, spaces
  - L232: boolean        - true, false, unknown
  - L245: Some of these characters, alone or in combination, may be used as operators (arithmetical, string,
  - L262: Comments may be present in SQL scripts, SQL statements and PSQL modules. A comment can be
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/03_Data_Types_and_Subtypes.md
- Potential ambiguity markers detected:
  - L13: comments may be of any length and can occupy multiple lines.
  - L58: number of characters is not specified,
  - L72: may not be able to access data beyond
  - L75: unknown
  - L93: not specified, 1 is used by default.
  - L112: precision is not specified, 34 is used by
  - L297: INT128 is a 128-bit integer data type. This type is not defined in the SQL standard.
  - L350: For testing data in columns with floating-point data types, expressions should check using a range,
  - L401: These non-standard type names are deprecated and may be removed in a
  - L515: fixed-point data should help to visualise why: for storage, the number is multiplied by 10 s
  - L645: stays the same, but the local time in the named zone may change.
  - L648: This may result in unexpected or confusing results.
  - L649: • When the rules of a named time zone changes, a value in the affected date range may no
  - L717:  Drivers may apply different defaults, for example specifying the client time zone
  - L865: this maximum, although it may affect the maximum size of any index that involves the column.
  - L878: encoding occupy 2 bytes in UTF8, characters from other encodings may occupy up
  - L898: constitutes any character: character encoding, collation, case, etc. are simply unknown. It is the
  - L902: Data in OCTETS encoding are treated as bytes that may not be interpreted as characters. OCTETS
  - L996:  Some tools may report the type as CHAR CHARACTER SET OCTETS instead of BINARY.
  - L1034:  Some tools may report the type as VARCHAR CHARACTER SET OCTETS  instead of
  - ... 24 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/04_Common_Language_Elements.md
- Potential ambiguity markers detected:
  - L17: VARBINARY — bytes; optional for fixed-width character types, defaults to 1
  - L33: expressions may include table columns, variables, constants, literals, various statements and
  - L41: An expression may contain a reference to an array member i.e., <array_name>[s], where s is the
  - L54: UNKNOWN} and IS [NOT] DISTINCT FROM
  - L137: • Care should be taken with the string length if the value is to be written to a CHAR
  - L201: case letters. Other client programs may use other conventions, such as displaying
  - L234: If necessary, a string literal may be preceded by a character set name, itself prefixed with an
  - L317: A Boolean literal is one of TRUE, FALSE or UNKNOWN.
  - L361: MM Month It may contain 1 or 2 digits (1-12 or 01-12). You can
  - L364: DD Day. It may contain 1 or 2 digits (1-31 or 01-31)
  - L365: HH Hour. It may contain 1 or 2 digits (0-23 or 00-23)
  - L366: mm Minutes. It may contain 1 or 2 digits (0-59 or 00-59)
  - L367: SS Seconds. It may contain 1 or 2 digits (0-59 or 00-59)
  - L368: NNNN Ten-thousandths of a second. It may contain from 1 to 4
  - L555: the optional ELSE clause is returned. If there are no matches and no ELSE clause, NULL is returned.
  - L567: ELSE 'Unknown'
  - L581: the optional ELSE clause is returned as the result. If no expressions return TRUE and there is no ELSE
  - L597: NULL is not a value in SQL, but a state indicating that the value of the element either is unknown or it
  - L601: and on other participating values. When you compare a value to NULL, the result will be unknown.
  - L602:  In SQL, the logical result unknown is also represented by NULL.
  - ... 22 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/05_DDL_Statements.md
- Potential ambiguity markers detected:
  - L11: regardless of the operator. This may appear to be contradictory, because every left-
  - L104: rolename The name of the role whose rights should be taken into account when
  - L127: SCHEMA. They are synonymous, but we recommend to always use CREATE DATABASE as this may change
  - L140: If the full path to the database is not specified, the database will be created in one of the system
  - L163: to create a database, the primary file specification should look like this:
  - L178: Optional Parameters for CREATE DATABASE
  - L192: changed to the closest smaller supported value. If the database page size is not specified, the
  - L212: character set NONE is used by default. Notice that the character set should be enclosed in a pair of
  - L222: The database page number at which the next secondary database file should start. When the
  - L358:  SCHEMA is currently a synonym for DATABASE; this may change in a future version, so
  - L378: the existing setting, you should delete the previously specified description of the delta file using
  - L573: page_num The number of the page at which the secondary shadow file should start
  - L576: Like a database, a shadow may be multi-file. The number and size of a shadow’s files are not
  - L589: lost one. It does not always succeed, however, and a new one may need to be created manually.
  - L598: deletes it using the DROP SHADOW statement. MANUAL should be selected if continuous shadowing is
  - L606: Specifies the shadow page number at which the next shadow file should start. The system will
  - L641: optional DELETE FILE clause makes this behaviour explicit. On the contrary, the PRESERVE FILE clause
  - L756: ◦ Two numbers separated by a colon (‘ :’) and optional whitespace, the second greater than
  - L760: separated by commas and optional whitespace.
  - L766: (SUB_TYPE TEXT) types. If the character set is not specified, the character set specified as DEFAULT
  - ... 94 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/06_DML_Statements.md
- Potential ambiguity markers detected:
  - L89: returned. The result set may be sorted by an ORDER BY clause, and FIRST … SKIP, OFFSET … FETCH or
  - L90: ROWS may further limit the number of returned rows, and can — for example — be used for
  - L92: The column list may contain all kinds of expressions, not only column names, and the source need
  - L93: not be a table or view: it may also be a derived table, a common table expression (CTE) or a
  - L94: selectable stored procedure. Multiple sources may be combined in a JOIN, and multiple result sets
  - L95: may be combined in a UNION.
  - L124: FIRST and SKIP are both optional. When used together as in “ FIRST m SKIP n”, the n topmost rows of
  - L216: The column list may optionally be preceded by one of the keywords DISTINCT or ALL:
  - L222: However, if the specified collation changes the case or accent sensitivity of the column, it may
  - L301: As you may have guessed, this will give you the default character set of the database.
  - L352: An alias may be useful or even necessary if there are subqueries that refer to the main select
  - L406: Values for optional parameters (that is, parameters for which default values have been defined)
  - L407: may be omitted or provided. However, if you provide them only partly, the parameters you omit
  - L409: Supposing that the procedure visible_stars from the previous example has two optional
  - L432: procedure alias should be omitted:
  - L479: optional) can be used:
  - L501: such as when it is a constant or a run-time expression, it should be given an
  - L504: ◦ The list of column aliases is optional but, if it exists, it must contain an alias
  - L518: Depending on the values of a, b and c, each equation may have zero, one or two solutions. It is
  - L527: If we want to show the coefficients next to the solutions (which may not be a bad idea), we can alter
  - ... 93 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/07_PSQL_Statements.md
- Potential ambiguity markers detected:
  - L30: A PSQL module may contain declarations of local variables, subroutines and cursors, assignments,
  - L44: prefixed by a colon (‘ :’) in most situations. The colon is optional in statement syntax that is specific
  - L67: functions — the return type. Stored procedures and PSQL blocks may have input and output
  - L68: parameters. Functions may have input parameters and must have a scalar return type. Triggers do
  - L108: PSQL_statements Procedural SQL statements. Some PSQL statements may not be valid in all
  - L120: misc-info Optional string that is passed to the procedure in the external module
  - L122: The PSQL module body starts with an optional section that declares variables and subroutines,
  - L125: as a single unit of code. The main BEGIN…END block may contain any number of other BEGIN…END
  - L213: Selectable procedures may have input parameters, and the output set is specified by the RETURNS
  - L255: DSQL, using the EXECUTE BLOCK syntax. The header of a PSQL block may optionally contain input and
  - L256: output parameters. The body may contain local variables, cursor declarations and local routines,
  - L259: A PSQL block is not defined and stored as an object, unlike stored procedures and triggers. It
  - L358: are executed — also known as “firing order” — can be specified explicitly with the optional POSITION
  - L504: any valid SQL expression: it may contain literals, internal variable names, arithmetic, logical and
  - L668: The cursor can be forward-only (unidirectional) or scrollable. The optional clause SCROLL makes the
  - L677: • The optional FOR UPDATE clause can be included in the SELECT statement, but its absence does not
  - L679: • Care should be taken to ensure that the names of declared cursors do not conflict with any
  - L688: • The SELECT statement may contain parameters. For instance:
  - L696: changes during the execution of the loop, then its new value may — but not
  - L698: situations. If you really need this behaviour, then you should thoroughly test your
  - ... 30 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/08_Built_in_Scalar_Functions.md
- Potential ambiguity markers detected:
  - L371: it obvious that ATAN2(0, 0) is undefined.
  - L596: positive numbers and downward for negative numbers. With the optional scale argument, the
  - L714: The single argument variant returns the integer part of a number. With the optional scale
  - L841: Gives the length in bits of the input string. For multi-byte character sets, this may be less than the
  - L1041: The optional USING clause specifies the non-cryptographic hash algorithm to apply. When the USING
  - L1046: not specified
  - L1050: USING clause, or cryptographic hashes through CRYPT_HASH() — should be used for more reliable
  - L1183: When used on a BLOB, this function may need to load the entire object into memory.
  - L1184: Although it does try to limit memory consumption, this may affect performance if
  - L1211: Gives the length in bytes (octets) of the input string. For multi-byte character sets, this may be less
  - L1252: length of the replacement string. With the optional fourth argument, a different number of
  - L1266:  When used on a BLOB, this function may need to load the entire object into memory.
  - L1267: This may affect performance if huge BLOBs are involved.
  - L1303: optional third argument, the search starts at a given offset, disregarding any matches that may
  - L1306: • The optional third argument is only supported in the second syntax (comma
  - L1319:  When used on a BLOB, this function may need to load the entire object into memory.
  - L1320: This may affect performance if huge BLOBs are involved.
  - L1353:  When used on a BLOB, this function may need to load the entire object into memory.
  - L1354: This may affect performance if huge BLOBs are involved.
  - L1405:  When used on a BLOB, this function may need to load the entire object into memory.
  - ... 29 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/09_Aggregate_Functions.md
- Potential ambiguity markers detected:
  - L87: expr Expression. It may contain a table column, a constant, a variable, an
  - L124: expr Expression. It may contain a table column, a constant, a variable, an
  - L161: expr Expression. It may contain a table column, a constant, a variable, an
  - L165: separator Optional alternative separator, a string expression. Comma is the default
  - L172: • The optional separator argument may be any string expression. This makes it possible to specify
  - L177: • The ordering of the list values is undefined — the order in which the strings are concatenated is
  - L192: 1. Retrieving the list, order undefined:
  - L209: expr Expression. It may contain a table column, a constant, a variable, an
  - L240: expr Expression. It may contain a table column, a constant, a variable, an
  - L271: expr Numeric expression. It may contain a table column, a constant, a
  - L307: exprN Numeric expression. It may contain a table column, a constant, a
  - L339: exprN Numeric expression. It may contain a table column, a constant, a
  - L365: exprN Numeric expression. It may contain a table column, a constant, a
  - L387: expr Numeric expression. It may contain a table column, a constant, a
  - L416: expr Numeric expression. It may contain a table column, a constant, a
  - L446: expr Numeric expression. It may contain a table column, a constant, a
  - L476: expr Numeric expression. It may contain a table column, a constant, a
  - L517: y Dependent variable of the regression line. It may contain a table column,
  - L520: x Independent variable of the regression line. It may contain a table
  - L543: y Dependent variable of the regression line. It may contain a table column,
  - ... 13 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/10_Window_Functions.md
- Potential ambiguity markers detected:
  - L12: y Dependent variable of the regression line. It may contain a table column,
  - L15: x Independent variable of the regression line. It may contain a table
  - L34: The window functions are used with the OVER clause. They may appear only in the SELECT list, or the
  - L36: Firebird window functions may be partitioned and ordered.
  - L93: value-expression Expression. May contain a table column, constant, variable, expression,
  - L99: window-frame-exclusion An optional clause that excludes specific rows from the window frame.
  - L143: Like aggregate functions, that may operate alone or in relation to a group, window functions may
  - L203: may appear strange that 37.00 is repeated for the ids 1 and 5, but that is how it should work. The
  - L222: • With RANGE, the ORDER BY should specify exactly one expression, and that expression should be of
  - L692: expr Expression. May contain a table column, constant, variable, expression,
  - L734: expr Expression. May contain a table column, constant, variable, expression,
  - L737: expr. If offset is not specified, the default is 1. offset can be a column,
  - L804: expr Expression. May contain a table column, constant, variable, expression,
  - L844: expr Expression. May contain a table column, constant, variable, expression,
  - L853: expr. If offset is not specified, the default is 1. offset can be a column,
  - L895: expr Expression. May contain a table column, constant, variable, expression,
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/11_System_Packages.md
- Potential ambiguity markers detected:
  - L51: BLOB_APPEND always creates BLOBs in temporary storage, which may not always be the best
  - L54: The BLOB returned from this function, even when TEMP_STORAGE = FALSE , may be used with
  - L84: MODE may be:
  - L88: When MODE is 2, OFFSET should be zero or negative.
  - L221: PLUGIN_OPTIONS are plugin specific options and currently should be NULL for the Default_Profiler
  - L261: and may be read and analyzed by the user.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/12_Context_Variables.md
- Potential ambiguity markers detected:
  - L113: The optional precision argument is not supported in ESQL.
  - L160: The optional precision argument is not supported in ESQL.
  - L297: The optional precision argument is not supported in ESQL.
  - L337: The optional precision argument is not supported in ESQL.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/13_Transaction_Control.md
- Potential ambiguity markers detected:
  - L79: tr_option Optional transaction option. Each option should be specified at most
  - L106: All clauses in the SET TRANSACTION statement are optional. If the statement starting a transaction has
  - L113:  Database drivers or access components may use different defaults for transactions
  - L121: TRANSACTION is either not supported, or may result in unspecified behaviour. An
  - L126: The optional NAME attribute defines the name of a transaction. Use of this attribute is available only
  - L135: • lock resolution mode (WAIT, NO WAIT) with an optional LOCK TIMEOUT specification
  - L157: When several client processes work with the same database, locks may occur when one process
  - L160: Locks may occur in other situations when multiple transaction isolation levels are used.
  - L212: different attachments) reading consistent data from a database. For example, a backup process may
  - L213: create multiple threads reading data from the database in parallel, or a web service may dispatch
  - L276: The other two variants can result in statement-level inconsistent reads as they may read some
  - L337: no special actions should be taken by developers to handle it in any way.
  - L486: • The optional TRANSACTION tr_name clause, available only in Embedded SQL, specifies the name of
  - L542: • The optional TRANSACTION tr_name clause, available only in Embedded SQL, specifies the name of
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/14_Security.md
- Potential ambiguity markers detected:
  - L98:  The default password “masterkey” is known across the universe. It should be
  - L112: security database and the password file may be different from the standard one.
  - L207:  Depending on the administrative status of the current user, more parameters may
  - L302:  Depending on the administrative status of the current user, more parameters may
  - L383: tasks it may be necessary to give the user more than one privilege to perform some
  - L473: firstname Optional: User’s first name. Maximum length 32 characters
  - L474: middlename Optional: User’s middle name. Maximum length 32 characters
  - L475: lastname Optional: User’s last name. Maximum length 32 characters.
  - L498: The optional FIRSTNAME, MIDDLENAME and LASTNAME clauses can be used to specify additional user
  - L519: applied when this clause is not specified.
  - L538: this may not be the case.
  - L592: Any user can alter their own account, except that only an administrator may use GRANT/REVOKE
  - L594: All clauses are optional, but at least one other than USING PLUGIN must be present:
  - L596: • FIRSTNAME, MIDDLENAME and LASTNAME update these optional user properties, such as the person’s
  - L638: 2. Editing the optional properties (the first and last names) of the user dan:
  - L674: statement must contain at least one of the optional clauses other than USING PLUGIN. If the user does
  - L702: The optional USING PLUGIN clause explicitly specifies the user manager plugin to use for dropping
  - L705: configuration) is applied when this clause is not specified.
  - L739: to multiple users in a single GRANT statement. Privileges may be revoked from a user with REVOKE
  - L1000:  SCHEMA is currently a synonym for DATABASE; this may change in a future version, so
  - ... 21 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/15_Management_Statements.md
- Potential ambiguity markers detected:
  - L49: should not be confused with DDL ALTER statements that modify database objects
  - L96: The special type LEGACY is used when a data type, missing in previous Firebird version, should be
  - L356: the plugin and a local or global mapping to a role for the current database. The role may be one
  - L463: is set. It is configurable per-database, so it may be set globally in firebird.conf and overridden
  - L623: Configures whether the optimizer should optimize for fetching first or all rows.
  - L669: ◦ In general, configuration values should revert to the values configured using the DPB at
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_A_Supplementary_Info.md
- Potential ambiguity markers detected:
  - L45: In this Appendix are topics that developers may wish to refer to, to enhance understanding of
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_B_Exception_Codes.md
- Potential ambiguity markers detected:
  - L62: SQLCODE has been used for many years and should be considered as deprecated now. Support for
  - L114: 08007 Transaction resolution unknown
  - L257: 40003 Statement completion unknown
  - L330: HYC00 Optional feature not implemented
  - L340:  SQLCODE has been used for many years and should be considered as deprecated
  - L443: -103 335544571 dsql_constant_err Data type for constant unknown
  - L448: -104 335544426 ctxnotdef context not defined (BLR error)
  - L449: -104 335544429 badparnum undefined parameter number
  - L464: -104 335544591 dsql_tran_err Unknown transaction option
  - L467: -104 335544612 token_err Token unknown
  - L468: -104 335544634 dsql_token_unk_err Token unknown - line @1, column @2
  - L521: -104 335545040 cp_name_too_long Length of crypt plugin name should not
  - L661: -172 335544438 funnotdef function @1 is not defined
  - L665: -204 335544463 gennotdef generator @1 is not defined
  - L667: -204 335544509 charset_not_found CHARACTER SET @1 is not defined
  - L668: -204 335544511 prcnotdef procedure @1 is not defined
  - L669: -204 335544515 codnotdef status code @1 unknown
  - L670: -204 335544516 xcpnotdef exception @1 not defined
  - L684: -204 335544573 dsql_datatype_err Data type unknown
  - L685: -204 335544580 dsql_relation_err Table unknown
  - ... 46 more matches
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_C_Reserved_Words.md
- Potential ambiguity markers detected:
  - L46: should avoid this unless you have a compelling reason.
  - L129: UNIQUE UNKNOWN UPDATE
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_D_System_Tables.md
- Potential ambiguity markers detected:
  - L30: UNKNOWN UPDATE UPDATING
  - L171: RDB$DESCRIPTION BLOB TEXT Optional description of the mapping
  - L211: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L248: trailing spaces should be taken into
  - L269: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L416: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L605: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L668: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L747: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L882: RDB$DESCRIPTION BLOB TEXT Optional description of the function
  - L901: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L1016: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L1025: RDB$DESCRIPTION BLOB TEXT Optional description of the package
  - L1066: RDB$SECURITY_CLASS CHAR(63) May point to the security class defined
  - L1072: may or may not be the username of the
  - L1262: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L1349: view. It should always be treated as
  - L1362: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L1393: RDB$SECURITY_CLASS CHAR(63) May reference a security class defined
  - L1469: not a distributed transaction. It may be
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_E_Monitoring_Tables.md
- Potential ambiguity markers detected:
  - L181: “Cache Writer” connections may report
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_G_Plugin_Tables.md
- Potential ambiguity markers detected:
  - L54: may use different table names.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/reference/firebird/firebird_docs_split/App_J_Document_History.md
- Potential ambiguity markers detected:
  - L13: (the “License”); you may only use this Documentation if you comply with the terms of this License.
  - L73: 0.3 26 May
  - L81: • Example for RDB$ROLE_IN_USE() should use RDB$ROLES (#184)
  - L103: 0.2 10 May
  - L126: 0.1 05 May
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/01-CORE_TYPES.md
- Potential ambiguity markers detected:
  - L460: {SQL_NULL, ScratchBirdType::UNKNOWN},
  - L628: constexpr const char* SERVER_NOT_FOUND      = "RD030";  // Foreign server not defined
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/02-CONNECTION_POOL.md
- Potential ambiguity markers detected:
  - L19: **Scope Note:** MSSQL/TDS adapter support is optional extension; MSSQL references are forward-looking.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/03-POSTGRESQL_ADAPTER.md
- Potential ambiguity markers detected:
  - L35: Startup messages may be preceded by SSLRequest or GSSENCRequest. If the server replies with an ErrorResponse to those requests, close the connection and retry without encryption (do not surface the error to users).
  - L583: "Unknown prepared statement: " + std::to_string(stmt_id));
  - L704: // Unknown type - return as text
  - L1016: std::string detail;        // Optional detail
  - L1017: std::string hint;          // Optional hint
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/04-MYSQL_ADAPTER.md
- Potential ambiguity markers detected:
  - L448: // Unknown plugin - try sending password directly (will likely fail)
  - L637: "Unknown prepared statement");
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/05-MSSQL_FIREBIRD_ADAPTERS.md
- Potential ambiguity markers detected:
  - L469: // Skip unknown token
  - L874: - Servers may use `op_accept_data` / `op_cond_accept` with plugin data; follow up with `op_cont_auth` exchanges until authentication completes.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/06-QUERY_EXECUTION.md
- Potential ambiguity markers detected:
  - L129: // Filters that may be pushed down
  - L136: std::optional<uint64_t> limit;
  - L137: std::optional<uint64_t> offset;
  - L181: std::optional<uint64_t> local_limit;
  - L182: std::optional<uint64_t> local_offset;
  - L543: std::optional<uint64_t> limit,
  - L544: std::optional<uint64_t> offset);
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/07-SCHEMA_INTROSPECTION.md
- Potential ambiguity markers detected:
  - L114: std::optional<PrimaryKeyInfo> primary_key;
  - L117: std::optional<PartitionInfo> partition_info;
  - L120: std::optional<std::string> view_definition;
  - L425: std::optional<std::string> table_prefix;
  - L426: std::optional<std::string> table_suffix;
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/08-SQL_SYNTAX.md
- Potential ambiguity markers detected:
  - L11: **Scope Note:** MSSQL/TDS adapter support is optional extension; MSSQL references are forward-looking.
  - L26: - `TYPE`: Optional explicit type (`postgresql`, `mysql`, `mssql`, `firebird`, `scratchbird`)
  - L36: **Optional Options:**
  - L75: -- SQL Server (optional extension)
  - L590: | RD030 | Server not found | Unknown foreign server |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/remote_database_udr/README.md
- Potential ambiguity markers detected:
  - L30: **Scope Note:** MSSQL/TDS adapter support is optional extension; references to MSSQL are forward-looking.
  - L46: | [05-MSSQL_FIREBIRD_ADAPTERS.md](05-MSSQL_FIREBIRD_ADAPTERS.md) | MSSQL (TDS, optional extension) and Firebird adapters | ~700 lines |
  - L98: │  │  │  (native)   │  │  (native)   │  │ TDS (optional extension) │ │    │
  - L114: │  │ PostgreSQL   │  │ MySQL 5.7/8  │  │ MS SQL (optional extension) │    │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/sblr/Appendix_A_SBLR_BYTECODE.md
- Potential ambiguity markers detected:
  - L73: The module is a header plus sections. Offsets are relative to the start of the header. Sections may be omitted if their size is zero.
  - L196: ### 6.4 Symbol Table (optional)
  - L206: ### 6.5 Debug Info (optional)
  - L216: ### 6.6 Profile Data (optional)
  - L219: ### 6.7 Exception Table (optional)
  - L315: Any opcode not defined by the registry MUST be rejected by the executor.
  - L565: The registry may grow; executors MUST reject unknown types unless explicitly configured for forward compatibility.
  - L585: - A system-wide policy may forbid non-transportable code; in that case the compiler MUST emit transportable SBLR even if requested otherwise.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/sblr/FIREBIRD_BLR_FIXTURES.md
- Potential ambiguity markers detected:
  - L152: cursor declarations, and parameter metadata. The translator should bind
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/sblr/FIREBIRD_BLR_TO_SBLR_MAPPING.md
- Potential ambiguity markers detected:
  - L111: | blr_matching / blr_matching2 / blr_similar | regex-like | `Opcode::EXT_REGEX_*` | may need dedicated opcode |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md
- Potential ambiguity markers detected:
  - L19: **Legacy term note:** This document may use xmin/xmax labels for creator/deleter transaction IDs. ScratchBird does not use PostgreSQL tuple headers; see the authoritative MGA specs above for the actual Firebird-style record header fields (e.g., `rhd_transaction`, `rhd_back_version`) and visibility/GC rules.
  - L137: - May still be reading/writing data
  - L376: - May not advance OIT/OAT markers
  - L520: - May cause index page consolidation
  - L574: // Check if sweep should trigger
  - L845: - Different statements in same transaction may see different data
  - L878: - May see inconsistent data within single statement
  - L887: -- some rows may get new value, others old value + 100
  - L1321: 2. Implement background GC (optional)
  - L1552: - [Code Audit Report](../Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/audit_2025_10_06.md) - Current implementation status
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/sblr/SBLR_EXECUTION_PERFORMANCE_BETA.md
- Potential ambiguity markers detected:
  - L17: - Provide optional vectorized execution for eligible workloads.
  - L22: - Mandatory JIT/AOT for all workloads (must be optional).
  - L29: ### Tier 3: Native Execution (Optional)
  - L31: - Optional AOT compilation for frequently called PSQL modules.
  - L34: ### Vectorized Execution (Optional)
  - L48: ### B) AOT Compilation (Optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/sblr/SBLR_EXECUTION_PERFORMANCE_RESEARCH.md
- Potential ambiguity markers detected:
  - L32: ### Oracle Database (PL/SQL compilation + optional native code)
  - L106: - AOT option for frequently called functions (similar to Oracle/PG optional JIT).
  - L175: ### Beta (Optional, High-Impact)
  - L178: - Optional AOT for frequently called PSQL modules.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/sblr/SBLR_OPCODE_REGISTRY.md
- Potential ambiguity markers detected:
  - L22: - Unknown opcodes MUST be rejected by the executor.
  - L25: Opcode values should be grouped to keep the codebase readable and debuggable.
  - L45: Existing assignments that do not fit these ranges remain valid; new assignments should conform.
  - L189: - TIME/TIMESTAMP literals may include optional timezone offsets in the parser.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L258: timestamp_t starts_at;               // Optional start time (ms)
  - L259: timestamp_t ends_at;                 // Optional end time (ms)
  - L260: string schedule_tz;                  // Time zone name (optional, default UTC)
  - L272: UUID run_as_role_uuid;               // Optional role override
  - L303: bytes result_data;                   // Optional result data
  - L491: [CLASS = job_class]              -- Optional, ignored in Alpha
  - L492: [PARTITION BY partition_rule]    -- Optional, ignored in Alpha
  - L494: [DEPENDS ON job_name [, ...]]    -- Optional, job dependencies
  - L495: [MAX_RETRIES = n]                -- Optional, default: 3
  - L496: [RETRY_BACKOFF = duration]       -- Optional, default: 60s
  - L497: [TIMEOUT = duration]             -- Optional
  - L498: [ON COMPLETION PRESERVE | DROP]  -- Optional (AT jobs)
  - L499: [RUN AS role_name]               -- Optional
  - L500: [DESCRIPTION = 'description']    -- Optional
  - L501: [ENABLED | DISABLED]             -- Optional
  - L619: starts_at BIGINT,                       -- Optional start time (ms)
  - L620: ends_at BIGINT,                         -- Optional end time (ms)
  - L621: schedule_tz VARCHAR(64),                -- Time zone name (optional, default UTC)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/scheduler/README.md
- Potential ambiguity markers detected:
  - L114: starts_at BIGINT,                       -- Optional start time (ms)
  - L115: ends_at BIGINT,                         -- Optional end time (ms)
  - L116: schedule_tz VARCHAR(64),                -- Time zone name (optional, default UTC)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md
- Potential ambiguity markers detected:
  - L81: - **Scheduler**: Component that decides when jobs should run.
  - L156: starts_at BIGINT,                       -- Optional start time (ms)
  - L157: ends_at BIGINT,                         -- Optional end time (ms)
  - L158: schedule_tz VARCHAR(64),                -- Time zone name (optional, default UTC)
  - L172: run_as_role_uuid UUID,                  -- Optional role override
  - L228: ### 6.4 sys.job_secrets (optional, for external jobs)
  - L336: - Optional config: catch_up = none | last | all.
  - L381: - Optional RUN AS role requires grant and is audited.
  - L392: - optional chroot/namespace isolation
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/ARCHITECTURE_CLARIFICATIONS.md
- Potential ambiguity markers detected:
  - L455: startup_database.sales = null  (or not specified)
  - L533: // 5. Cluster Manager Thread (optional)
  - L567: │    ├──▶ Cluster Thread (optional) │
  - L620: │  │  - May leave locks dangling     │   │
  - L629: - Bad connection may ignore request
  - L631: - Locks may not be released properly
  - L915: - Server: "Last query status: UNKNOWN (server error)"
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/BACKUP_AND_RESTORE.md
- Potential ambiguity markers detected:
  - L21: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L22: Any WAL references in this document describe an optional extension stream for
  - L24: **Table Footnote:** In comparison tables below, ScratchBird WAL references are optional extension (replication/PITR).
  - L108: ScratchBird uses Firebird-style Multi-Generational Architecture (MGA), NOT PostgreSQL-style write-after log (WAL, optional extension):
  - L110: - **No write-after log (WAL, optional extension) for Recovery** - Backup/restore does not rely on write-after log (WAL, optional extension) replay
  - L115: **Critical:** Backup consistency is achieved through MGA snapshot isolation, NOT through write-after log (WAL, optional extension) checkpoints.
  - L124: ├── PITR Archive (optional, low priority)
  - L141: - **Write-after log (WAL) Archive** for PITR is optional and low-priority feature
  - L245: - May include dead tuples (pre-sweep)
  - L309: | Index (optional)          |
  - L454: ### 4.5. Page Index (Optional)
  - L491: | 80 | 64 | `uint8_t[64]` | **Digital Signature** | Ed25519 signature (optional) |
  - L555: **Critical:** ScratchBird uses MGA snapshot isolation, NOT write-after log (WAL, optional extension) checkpoints.
  - L823: │ 6. REBUILD INDEXES (optional)                            │
  - L1003: **Note:** Transaction log archive is distinct from write-after log (WAL, optional extension), which ScratchBird does not use for recovery.
  - L1145: 3. **Old Versions** - Backup may include dead tuples (before sweep)
  - L1183: **Issue:** Backup may include dead tuples if sweep has not run.
  - L2390: # Upload to S3 (optional)
  - L2565: | **Continuous** | Transaction Log | 7 days | PITR (optional) |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/DATABASE_REGISTRY_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L65: database_alias      TEXT,                       -- Short alias (optional)
  - L157: expires_at          TIMESTAMP,                  -- Optional expiration
  - L344: --   - options: JSON with optional parameters
  - L460: IN session_stats JSON  -- Optional query stats from session
  - L513: std::optional<std::string> page_size;
  - L514: std::optional<std::string> charset;
  - L515: std::optional<std::string> security_model;
  - L516: std::optional<std::string> description;
  - L563: std::optional<DatabaseInfo> getDatabaseByName(
  - L566: std::optional<DatabaseInfo> getDatabaseById(
  - L805: - Registry should be backed up along with databases
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md
- Potential ambiguity markers detected:
  - L81: │   GC HANDLED BY SERVER (or optional background thread)                     │
  - L238: #include <optional>
  - L251: std::optional<int64_t> buffer_pool_size;
  - L252: std::optional<int> max_connections;
  - L304: std::optional<DatabaseInfo> getDatabaseById(const core::UUID& db_id);
  - L305: std::optional<DatabaseInfo> getDatabaseByName(const std::string& name);
  - L306: std::optional<DatabaseInfo> getDatabaseByAlias(const std::string& alias);
  - L527: std::optional<DatabaseInfo> DatabaseRegistry::getDatabaseByName(
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/DRIVER_STREAMING_AND_PAGING.md
- Potential ambiguity markers detected:
  - L28: - Drivers should support repeated EXECUTE calls to fetch subsequent pages.
  - L29: - Where possible, drivers should expose a `fetch_size` (rows per page) option.
  - L33: - Large values may be returned as streamed columns (length = -2 in DATA_ROW).
  - L48: - Optional: expose fetch size via driver-specific options.
  - L63: - ResultStream#each should yield rows incrementally.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L393: ; Client certificate verification: none, optional, require
  - L481: result.addWarning("network.native_port", "Port may be in use");
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/MEMORY_MANAGEMENT.md
- Potential ambiguity markers detected:
  - L21: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in V3. WAL is disabled for recovery; if enabled, it exists only as an optional replication/PITR stream.
  - L22: Any WAL references in this document describe an optional replication/PITR stream for
  - L24: **Table Footnote:** In comparison tables below, ScratchBird WAL references are optional extension (replication/PITR).
  - L104: │   - jemalloc (optional)                              │
  - L193: **ScratchBird uses Firebird MGA, not PostgreSQL write-after log (WAL, optional extension):**
  - L195: - **No write-after log (WAL, optional extension) Buffers** - MGA doesn't require write-after log (WAL, optional extension) for recovery
  - L443: - Multiple threads may allocate concurrently
  - L946: ### 6.2. jemalloc (Optional)
  - L1552: // Destroy context (should free all allocations)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/PERFORMANCE_BENCHMARKS.md
- Potential ambiguity markers detected:
  - L20: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L21: Any WAL references in this document describe an optional extension stream for
  - L23: **Table Footnote:** In comparison tables below, ScratchBird WAL references are optional extension (replication/PITR).
  - L81: - **Multi-Version In-Page** - Page updates may be larger than PostgreSQL
  - L95: Note: Optional write-after log (WAL) for replication/PITR may reintroduce write-after log (WAL)-style overhead.
  - L127: Storage: 1 TB NVMe SSD (RAID 10 optional)
  - L610: # Write-after log (WAL, optional; not used for recovery in MGA)
  - L611: wal_level = minimal                 # MGA doesn't need write-after log (WAL, optional extension) for recovery
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/SCRATCHBIRD_ARCHITECTURE_OVERVIEW.md
- Potential ambiguity markers detected:
  - L119: │  │  │  │  (optional)  │  │              │  │                                             │  │   │
  - L267: │   Optional               │     │  • Execute startup SQL scripts                          │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/SCRATCHBIRD_CONNECTION_RECOVERY_MODEL.md
- Potential ambiguity markers detected:
  - L140: - "Last query: FAILED/COMPLETED/UNKNOWN"
  - L176: | Query status unknown | Keep tx active | "Reconnected, triage required" |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L107: - Application responsible for GC (optional thread)
  - L198: nullptr                     // Options (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/SCRATCHBIRD_SECURITY_AND_ACCESS_MODEL.md
- Potential ambiguity markers detected:
  - L52: │ Database List    │ (or empty if none) │ (may be empty)     │ cluster config │
  - L62: │                  │ (full control)     │ server may limit)  │ cluster-wide   │
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md
- Potential ambiguity markers detected:
  - L99: - Cluster Manager (optional)
  - L120: ### Phase 3: Startup Databases (optional)
  - L199: **Why this design:** Long-running analytical work should not be lost due to transient connectivity; the engine can finish work and resume delivery once the session is restored.
  - L238: - Max reconnect attempts are configurable and should alert monitoring.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md
- Potential ambiguity markers detected:
  - L178: The database registry maintains a catalog of all databases known to the server instance. Each port listener may have its own registry (enabling MSSQL-style "named instances"). When a client connects, they receive a list of databases they can access from the registry.
  - L442: - Optional client certificate verification
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/server/TEMPORARY_TABLES_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L12: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in V3. WAL is disabled for recovery; if enabled, it exists only as an optional replication/PITR stream.
  - L13: Any WAL references in this document describe an optional replication/PITR stream for
  - L136: - Temporary table data MUST NOT be written to the optional write-after log stream.
  - L140: - Engine MAY store temporary table data in memory or a dedicated temp tablespace.
  - L156: - Syntax: `CREATE TEMP[TEMPORARY] TABLE` with optional ON COMMIT.
  - L161: - TEMP table name SHOULD shadow permanent table names for that session.
  - L211: - Session-scoped temp tables MUST preserve definition; data MAY be cleared
  - L218: Minimum enforced rules (dialect-specific extensions MAY apply):
  - L244: SELECT * FROM session_temp;  -- should fail
  - L266: SELECT * FROM drop_on_commit;  -- should fail
  - L272: SELECT * FROM my_temp;        -- should fail
  - L282: - Storage: use temp tablespace or in-memory storage, no optional extension write-after log (WAL).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/sprints/CONSTRAINT_ENFORCEMENT_SUMMARY.md
- Potential ambiguity markers detected:
  - L11: ## Quick Comparison: Nov 19 Audit vs Current Implementation
  - L210: - If CHECK expressions are large, they may be stored in TOAST
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/sprints/INDEX_INTEGRATION_SPRINT4_COMPLETE.md
- Potential ambiguity markers detected:
  - L245: 1. **Text Processing** (Phase 2 - Optional)
  - L251: 2. **Operators & Functions** (Phase 3 - Optional)
  - L256: 3. **Query Planner Integration** (Phase 4 - Optional)
  - L261: 4. **SQL Parser Integration** (Phase 5 - Optional)
  - L392: ### Optional Enhancements
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/sprints/SPRINT0_MGA_BUG_FIX.md
- Potential ambiguity markers detected:
  - L9: **Effort**: 2-4 hours (estimated, actual implementation time unknown)
  - L57: The current implementation in `src/core/storage_engine.cpp` (lines 878-1034) correctly implements Firebird MGA:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/sprints/SPRINT4_ONLINE_MIGRATION_INFRASTRUCTURE.md
- Potential ambiguity markers detected:
  - L16: Sprint 4 successfully implemented the core infrastructure for ONLINE table migration, providing the foundation for concurrent read/write access during migration. While the full ONLINE migration execution engine (copying, catch-up, swap) is deferred to optional extension, the infrastructure built here is production-ready and can be leveraged when ONLINE migration is prioritized.
  - L116: 3. **Query cache integration** (optional)
  - L238: ## What's NOT Implemented (Deferred to optional extension)
  - L323: ## Future Work (optional extension)
  - L360: This infrastructure can be leveraged immediately for offline migration and provides the foundation for future ONLINE migration implementation when prioritized optional extension.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/sprints/SPRINT5_EXECUTION_ENGINE.md
- Potential ambiguity markers detected:
  - L153: **Recommendation**: Proceed with BETA; fix circular dependency in optional extension cleanup
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/sprints/SPRINT6_ONLINE_MIGRATION_POLISH.md
- Potential ambiguity markers detected:
  - L36: - Current implementation does immediate cleanup after atomic swap
  - L98: - **Recommendation**: Add integration tests optional extension when test infrastructure exists
  - L112: **Recommendation**: Implement as part of optional extension test suite development.
  - L190: 2. **No automatic retry**: If cancel fails partway through, manual intervention may be needed
  - L191: 3. **No integration tests**: Code not tested with concurrent workloads (deferred to optional extension)
  - L248: **optional extension Work**:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_1.md
- Potential ambiguity markers detected:
  - L171: **Conclusion**: Task 1.2 complete because visibility filtering is already correctly implemented where it should be (heap layer), not where the original plan incorrectly assumed (index layer).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_2_5_TID_ANALYSIS.md
- Potential ambiguity markers detected:
  - L154: - Bitmap stores tuple positions, may need TID → position mapping
  - L158: - Should use GPID for block addressing (not TID)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_4.md
- Potential ambiguity markers detected:
  - L409: | Merge preserves MVCC semantics | ⏸️ DEFERRED | Optional enhancement for Phase 3 |
  - L445: - **Reason**: Deferred as optional enhancement
  - L446: - **Impact**: Current implementation sufficient for Alpha
  - L516: - Uncommitted entries may be merged
  - L519: - Uncommitted changes may become visible after merge (visibility bug)
  - L520: - Rolled-back transactions may leave entries in index
  - L606: - Large result sets (> 10000 TIDs): May become bottleneck (> 5 ms)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_5.md
- Potential ambiguity markers detected:
  - L413: - LRU eviction may thrash if result set spans many pages
  - L446: 1. **NULL Snapshot**: Should return all TIDs unfiltered
  - L447: 2. **READ COMMITTED**: Should see committed transactions
  - L448: 3. **REPEATABLE READ**: Should see snapshot-consistent view
  - L449: 4. **SERIALIZABLE**: Should respect serialization order
  - L450: 5. **Invalid TIDs**: Should skip gracefully (no crash)
  - L451: 6. **Empty Results**: Should return empty vector
  - L452: 7. **Dead Tuples**: Should filter out (offset=0 or length=0)
  - L453: 8. **Page Read Failures**: Should skip TID and continue
  - L463: 4. Rolled-back transactions (should not be visible)
  - L486: - Result set size vs. latency (should be linear)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE1_TASK1_6.md
- Potential ambiguity markers detected:
  - L54: - Query with xid2 snapshot should see tuple
  - L63: - xid2 queries, should NOT see tuple
  - L68: - xid3 queries, should NOT see tuple
  - L78: - xid2 queries, should NOT see pending entry
  - L80: - xid3 queries, SHOULD see entry
  - L86: - Query should return exactly 2 results (xid1 and xid3)
  - L95: - xid2 should still see tuple (snapshot before delete)
  - L96: - xid4 should NOT see tuple (snapshot after delete)
  - L100: - Query with NULL snapshot should work (READ COMMITTED fallback)
  - L264: Actual API may differ (signature not documented in test code)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_1.md
- Potential ambiguity markers detected:
  - L138: // Check if sweep should be triggered (OST - OIT > threshold)
  - L468: - Index GC is optional (correctness not affected)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_2.md
- Potential ambiguity markers detected:
  - L211: - `Status::IO_ERROR` - Had errors but may have removed some entries
  - L316: - ✅ Accepts nullptr for optional out parameters
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_4.md
- Potential ambiguity markers detected:
  - L277: - `Status::IO_ERROR` - Had errors but may have removed some entries
  - L386: - ✅ Accepts nullptr for optional out parameters
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_5.md
- Potential ambiguity markers detected:
  - L351: pages_modified = 1   (estimated, actual may be higher)
  - L402: - `Status::IO_ERROR` - Had errors but may have removed some entries
  - L517: - ✅ Accepts nullptr for optional out parameters
  - L531: **Current Implementation**: Truncates to lower 32 bits
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE2_TASK2_6.md
- Potential ambiguity markers detected:
  - L363: - Implemented `cleanIndexes()` placeholder with TODO (lines 771-830)
  - L432: - Full implementation plan documented in TODO comments
  - L531: - Remove TODO and implement full index iteration
  - L569: 2. ✅ `GarbageCollector::cleanIndexes()` - Framework implemented with clear TODO
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE3_AUTOEXTEND.md
- Potential ambiguity markers detected:
  - L9: **Implementation Date**: Unknown (was already complete)
  - L10: **Effort**: 12-18 hours (estimated, actual time unknown)
  - L429: - Deferred to optional extension
  - L434: - Deferred to optional extension
  - L438: - Deferred to optional extension
  - L491: - TBD (scope to be defined)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE3_TASK3_1_ARCHITECTURAL_DECISION.md
- Potential ambiguity markers detected:
  - L16: After careful architectural analysis, **TASK 3.1 (Add xmax Support Everywhere) should NOT be implemented** as specified in the INDEX_MGA_IMPLEMENTATION_PLAN.md. The task is based on a PostgreSQL-centric assumption that doesn't apply to ScratchBird's Firebird MGA architecture.
  - L207: | **Index Maintenance** | May need index updates on UPDATE | No updates unless indexed column changes |
  - L226: ### What Should Be Done Instead
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE3_TASK3_2_ARCHITECTURAL_ANALYSIS.md
- Potential ambiguity markers detected:
  - L99: **ALPHA Necessity**: ⚠️ **OPTIONAL**
  - L103: - Should be implemented for BETA or v1.0
  - L122: - **Status**: ⏸️ Optional for ALPHA, recommended for BETA
  - L133: **Reason**: 80% of the task (snapshot isolation) is already complete in Phase 1. The remaining 20% (SERIALIZABLE) is optional for ALPHA.
  - L137: **New TASK 3.2 (Optional for BETA)**: Implement SERIALIZABLE Isolation
  - L186: - ⚠️ SERIALIZABLE isolation - OPTIONAL (defer to BETA)
  - L214: 2. **20% remaining** (SERIALIZABLE predicate locking) is **optional for ALPHA**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE4A_TASK4A_1_BRIN_INDEX.md
- Potential ambiguity markers detected:
  - L289: 10. ✅ `RemoveDeadEntriesPartial` - Test partial range deletion (should NOT remove)
  - L290: 11. ✅ `RemoveDeadEntriesComplete` - Test complete range deletion (SHOULD remove)
  - L332: // Verify pruning: old blocks (< 900) should NOT be returned
  - L333: EXPECT_FALSE(found_old_block) << "Should prune old blocks effectively";
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE6_ATTACH_DETACH_COMPLETE.md
- Potential ambiguity markers detected:
  - L58: - Fields: `file_path`, `tablespace_name` (optional)
  - L74: - Optional AS clause for name override
  - L78: - Optional FORCE keyword
  - L295: 2. **Name conflicts**: ATTACH with duplicate name (should fail)
  - L296: 3. **Page size mismatch**: ATTACH incompatible tablespace (should fail)
  - L299: 6. **PRIMARY protection**: DETACH PRIMARY (should fail)
  - L312: SELECT COUNT(*) FROM archived_orders;  -- Should work
  - L329: - Mitigation: Tables should still be queryable if page structure is compatible
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/status/tablespace/PHASE6_ATTACH_DETACH_PARTIAL.md
- Potential ambiguity markers detected:
  - L333: - TODO: Update pg_tablespace record on disk (mark is_valid=0)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/storage/HEAP_TOAST_INTEGRATION.md
- Potential ambiguity markers detected:
  - L19: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L20: Any WAL references in this document describe an optional extension stream for
  - L40: The `HeapPage` class has been extended with an optional constructor that accepts:
  - L60: 3. **Tuple Fallback**: If the row still exceeds page limits, tuple-level TOAST may be used as a fallback
  - L73: 1. **get_tuple()**: Returns the raw tuple data (may contain TOAST pointer)
  - L77: // Get raw tuple (may contain TOAST pointer)
  - L155: 3. **Monitor TOAST Tables**: TOAST tables can grow large and may need maintenance
  - L172: - va_extsize: Stored size (may be compressed)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/storage/MGA_IMPLEMENTATION.md
- Potential ambiguity markers detected:
  - L65: - Record access paths may prune back-versions when safe.
  - L77: - Sweep uses a read-committed transaction and may run in the background.
  - L125: The snapshot may be TIP-based (traditional) or CN-based (commit-order) for read
  - L221: until GC can process it. ScratchBird should preserve this behavior for large scans.
  - L248: - Version chain depth thresholds should be configurable to avoid long stalls on
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/storage/ON_DISK_FORMAT.md
- Potential ambiguity markers detected:
  - L18: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L19: Any WAL references in this document describe an optional extension stream for
  - L63: uint64_t lsn;            // 0x10: Log Sequence Number (0 if no optional extension WAL)
  - L148: uint32_t wal_level;          // write-after log (WAL) level (0=none for Alpha; reserved for optional extension WAL)
  - L348: | Special Area     | (optional, e.g., for indexes)
  - L408: test.db.wal      - Write-after log (WAL, optional extension) (future)
  - L430: - Update LSN (only when optional write-after log (WAL) is enabled optional extension)
  - L474: - The system catalog root may point to additional catalog pages when full; Alpha may store only minimal entries and expand in later phases.
  - L492: - Use `O_DIRECT` for bypassing page cache (optional)
  - L629: 5. Compression is optional (EXTERNAL strategy)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/storage/STORAGE_ENGINE_BUFFER_POOL.md
- Potential ambiguity markers detected:
  - L37: **Configurable toggles (MUST be implemented, MAY be disabled):**
  - L157: BP_LOG              // Reserved for optional write-after log (not used in MGA)
  - L649: AP_UNKNOWN,         // Unknown pattern
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md
- Potential ambiguity markers detected:
  - L22: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L23: Any WAL references in this document describe an optional extension stream for
  - L47: // LSN info (optional write-after log only)
  - L48: LSN             pd_lsn;                // Last optional WAL LSN that modified page
  - L397: // Supports full MGA transaction recovery without write-after log (WAL, optional extension)
  - L481: // the page contains data and should not be reused until GC
  - L534: // 3. Load FSM from page 2 (may be stale after crash)
  - L542: // supporting full transaction recovery without write-after log (WAL, optional extension)
  - L568: **Why FSM Reconstruction (Not Write-after log (WAL, optional extension))?**
  - L570: ScratchBird uses **Firebird-style MGA (Multi-Generational Architecture)**, which provides crash recovery without requiring a write-after log (WAL, optional extension) stream for this purpose:
  - L584: - **No write-after log (WAL, optional extension) needed for crash recovery**
  - L616: **Note on Write-after log (WAL, optional extension) Purpose:**
  - L617: - **Write-after log (WAL, optional extension) is NOT needed for crash recovery** in MGA (Firebird proves this)
  - L618: - Write-after log (WAL, optional extension) is valuable for:
  - L622: - ScratchBird may add write-after log (WAL, optional extension) in Beta for replication support
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/storage/TABLESPACE_ONLINE_MIGRATION.md
- Potential ambiguity markers detected:
  - L16: **Status:** Beta specification (optional extension)
  - L250: - Optional Beta: Tablespace shrink/compaction (MGA-safe):
  - L251: `/docs/specifications/parser/v3/beta_requirements/optional/TABLESPACE_SHRINK_COMPACTION.md`
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/storage/TABLESPACE_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L996: - Transaction ID conflicts (OIT/OST from source may conflict with target)
  - L1028: - Hot tablespace (SSD) may benefit from dedicated buffer pool (optional extension)
  - L1032: **Impact**: Indexes may reside in different tablespace than heap.
  - L1054: 2. **Hot Backup** (optional): Use copy-on-write or snapshot mechanisms per tablespace (see backup specs)
  - L1061: **Optional**: Support relocating tablespace files at restore time (similar to PostgreSQL tablespace_map)
  - L1190: ### Optional Extensions
  - L1192: The following are optional features. If implemented, they MUST follow this spec:
  - L1318: - AUTOEXTEND size should be large enough to reduce extend frequency (reduces fragmentation)
  - L1369: 8. Implement optional extensions as needed
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/storage/TOAST_LOB_STORAGE.md
- Potential ambiguity markers detected:
  - L22: **WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional extension (replication/PITR).
  - L23: Any WAL references in this document describe an optional extension stream for
  - L25: **Table Footnote:** In comparison tables below, ScratchBird WAL references are optional extension (replication/PITR).
  - L221: **Without write-after log (WAL, optional extension)** (MGA approach):
  - L229: **NO write-after log (WAL, optional extension) replay needed** - all state recovered from TIP.
  - L364: - Larger chunks: Less overhead, may waste space
  - L425: - ✅ **Crash recovery (TIP state, no write-after log (WAL, optional extension))**
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/testing/ALPHA3_TEST_PLAN.md
- Potential ambiguity markers detected:
  - L73: | PG-CONN-003 | StartupMessage with unknown user | ErrorResponse (28000) |
  - L177: ### 3.3 TDS Protocol - SQL Server (Port 1433, optional extension)
  - L179: **Scope Note:** TDS/MSSQL testing is optional extension and is not part of Alpha validation. This section is retained for future coverage.
  - L294: | NTLM | TDS (optional extension) | P1 |
  - L308: | AUTH-003 | Password | Unknown user | Failure (28000) |
  - L386: | LOAD-TDS-001 | TDS (optional extension) | BCP bulk load | 100K rows/s |
  - L468: | SEC-FUZZ-003 | TDS protocol fuzz (optional extension) | Custom fuzzer | Crash/RCE |
  - L585: | TDS (optional extension) | ±10% | ±10% | ±15% |
  - L616: protocol: [postgresql, mysql, firebird, native]  # tds optional extension
  - L649: │   ├── tds.xml (optional extension)
  - L672: | 3 | TDS + Firebird protocols (optional extension) | TDS-*, FB-* |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/testing/test_server/README.md
- Potential ambiguity markers detected:
  - L203: # Should prompt for SCRAM challenge-response
  - L245: 1. Attempt connection from remote host (should fail)
  - L246: 2. Attempt connection with wrong auth method (should fail)
  - L247: 3. Attempt local connection with correct method (should succeed)
  - L260: 2. Attempt 6th login (should be locked)
  - L262: 4. Attempt login again (should succeed)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/testing/test_server/SECURITY_TESTING.md
- Potential ambiguity markers detected:
  - L373: - Login may succeed
  - L493: # 6th attempt should be locked
  - L494: echo "Attempt 6: Should be locked..."
  - L500: echo "Attempt after wait: Should succeed..."
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/tools/SB_BACKUP_CLI_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L73: -d, --database <db>     Database name (optional if provided positionally)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/tools/SB_ISQL_CLI_SPECIFICATION.md
- Potential ambiguity markers detected:
  - L89: - In **local/IPC mode** (if enabled), `<database>` may be a filesystem path.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/tools/SB_TOOLING_NETWORK_SPEC.md
- Potential ambiguity markers detected:
  - L26: Some tools may optionally spawn a local helper process.
  - L35: -d, --database <db>     Database name (optional, tool-specific)
  - L36: -r, --role <role>       Role name (optional)
  - L54: - If a shared listener is enabled, tools may target that port instead.
  - L60: - In local mode, `<database>` may be a filesystem path.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/transaction/FIREBIRD_CONSTANTS_REFERENCE.md
- Potential ambiguity markers detected:
  - L9: ScratchBird implementation. Values are taken from Firebird sources and should be
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md
- Potential ambiguity markers detected:
  - L21: may be garbage if no snapshot requires them.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/transaction/TRANSACTION_DISTRIBUTED.md
- Potential ambiguity markers detected:
  - L9: optional; if disabled, any distributed transaction request MUST be rejected.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/triggers/TRIGGER_CONTEXT_VARIABLES.md
- Potential ambiguity markers detected:
  - L11: Triggers may call procedures or functions that accept cursor handle parameters.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/03_TYPES_AND_DOMAINS.md
- Potential ambiguity markers detected:
  - L24: | **Date/Time** | DATE, TIME, TIMESTAMP, INTERVAL, TIME WITH TIME ZONE, TIMESTAMP WITH TIME ZONE | Types for storing dates, times of day, timestamps (with optional time zone), and time intervals. |
  - L32: - **Arrays:** SQL arrays are homogeneous and may be multi-dimensional. The on-disk encoding is a typed element list with an explicit dimension header. The header MUST include:
  - L37: - `null_bitmap` (optional, one bit per element; omitted if no NULLs)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/BINARY_LAYOUT_ANNEX.md
- Potential ambiguity markers detected:
  - L74: optional null bitmap
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/CANONICALIZATION_RULES.md
- Potential ambiguity markers detected:
  - L28: - `tz_name` MAY be empty to indicate “offset only”.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/COLLATION_RUNTIME_FORMAT.md
- Potential ambiguity markers detected:
  - L96: ### 2.5 Casefold Table (Optional)
  - L118: - Quaternary weights (optional).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md
- Potential ambiguity markers detected:
  - L67: - JSONB is a varlena container of JEntry headers and value data. The root node is an array or object, with JB_FARRAY/JB_FOBJECT flags and optional JB_FSCALAR for scalar values.
  - L103: - optional null bitmap
  - L157: - TEXT and BLOB/BYTEA lengths may exceed 32,765 bytes and are stored via TOAST.
  - L186: - Separators between date fields may be any punctuation; whitespace between components is allowed.
  - L196: - Integer: optional sign, base-10 digits. Hex (`0x` or `0X`) accepted with USING hexadecimal.
  - L197: - Floating: base-10 with optional exponent, finite only.
  - L198: - DECIMAL/NUMERIC: optional sign, digits, optional decimal point; scale/precision enforced by target type.
  - L206: - Optional prefixes: `0x` or `\\x` for hex.
  - L248: - analyze(value) -> stats (optional)
  - L275: ## Appendix: Storage Format v2 (Beta Optional)
  - L284: ### Packed NUMERIC (Optional)
  - L285: - NUMERIC may use a packed base-10000 digit format when configured (`numeric_storage = packed`).
  - L286: - DECIMAL always remains scaled-integer encoding; NUMERIC with precision <= 38 may remain scaled unless configured to packed.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/MULTI_GEOMETRY_TYPES_SPEC.md
- Potential ambiguity markers detected:
  - L71: - Points may occupy the same location (non-simple)
  - L99: - LineStrings may be disconnected (gaps allowed)
  - L101: - LineStrings may intersect at points
  - L106: - May form disconnected segments
  - L136: - Polygons may touch only at finite points
  - L137: - Each polygon may have holes (interior rings)
  - L142: - Polygons may share boundary points but not boundary segments
  - L143: - No polygon may be contained within another
  - L177: - Elements may overlap, touch, or be disjoint
  - L389: int32_t srid;  // Spatial Reference ID (0 = undefined)
  - L504: // Polygons may only touch at points, not edges
  - L594: - ✓ May be empty
  - L600: - ✓ May be empty
  - L607: - ✓ May be empty
  - L610: - ✗ Polygons may touch only at points
  - L611: - ✗ No polygon may contain another
  - L616: - ✓ May be empty
  - L620: - ✓ All geometries should have same SRID (warning if not)
  - L682: 5. **Validation Functions** (SHOULD HAVE)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/POSTGRESQL_ARRAY_TYPE_SPEC.md
- Potential ambiguity markers detected:
  - L39: ### Current Implementation Status
  - L326: auto getElement(size_t flat_index) const -> std::optional<Element>;
  - L328: auto at(const std::vector<size_t>& indices) const -> std::optional<Element>;
  - L333: -> std::optional<ArrayValue>;
  - L335: -> std::optional<ArrayValue>;
  - L337: auto transpose() const -> std::optional<ArrayValue>;  // 2D only
  - L340: auto getInt32Vector() const -> std::optional<std::vector<int32_t>>;
  - L341: auto getInt64Vector() const -> std::optional<std::vector<int64_t>>;
  - L369: -> std::optional<ArrayValue>;
  - L374: -> std::optional<ArrayValue>;
  - L382: -> std::optional<ArrayValue>;
  - L387: size_t axis) -> std::optional<ArrayValue>;
  - L585: The existing implementation should follow PostgreSQL format:
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md
- Potential ambiguity markers detected:
  - L9: | UNKNOWN | 0x0B00 | SBLR3_TYPE_UNKNOWN |
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/TIMEZONE_SYSTEM_CATALOG.md
- Potential ambiguity markers detected:
  - L243: **Current Implementation**: Hardcoded timezones in `TimezoneManager`
  - L263: must be loaded into `pg_timezone` using the loader tool and the catalog should
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md
- Potential ambiguity markers detected:
  - L76: optional null bitmap
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/types/character_sets_and_collations.md
- Potential ambiguity markers detected:
  - L427: 4. ICU library integration (optional)
  - L437: 2. **Character count** - for SQL semantics (optional, can be calculated)
  - L758: **Note**: Non-ASCII characters in unquoted identifiers may require parser updates (future enhancement).
  - L842: - Client applications should send/receive data in UTF-8 by default
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr/10-UDR-System-Specification.md
- Potential ambiguity markers detected:
  - L966: status_set_error(status, -2, "Unknown function");
  - L1092: // 1. Check file permissions (should not be world-writable)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr/UDR_PSQL_EXTENSION_LIBRARY.md
- Potential ambiguity markers detected:
  - L98: External libs: Boost.Math (preferred), Cephes (optional).
  - L146: External libs: FFTW (optional).
  - L160: Optional: day-count conventions, yield curve helpers, Black-Scholes.
  - L178: External libs: GEOS/PROJ (optional).
  - L202: - Determine UDR bundle signing and distribution strategy for optional packs.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr_connectors/firebird_udr/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L27: - Auth methods: SRP (preferred), legacy auth (optional).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr_connectors/jdbc_udr/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L28: - JDBC URL optional; DSN-less params supported
  - L36: | jdbc_url | "" | Optional JDBC URL |
  - L75: per the Remote Database UDR spec. JDBC drivers that expose catalogs should map
  - L80: - Migration: optional and explicit; uses the migration state machine defined in
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr_connectors/local_files_udr/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L12: - Optional write support if explicitly enabled (default read-only).
  - L23: - Capabilities: file_read (optional file_write)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr_connectors/local_scripts_udr/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L22: - Capabilities: subprocess + file_read (optional file_write)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr_connectors/mssql_udr/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L115: - Migration: optional and explicit; uses the migration state machine defined in
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr_connectors/mysql_udr/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L29: - Optional session settings: sql_mode, time_zone, character_set_results.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr_connectors/odbc_udr/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L28: - DSN optional; DSN-less params supported
  - L36: | dsn | "" | Optional DSN label |
  - L111: per the Remote Database UDR spec. ODBC drivers that expose catalogs should map
  - L116: - Migration: optional and explicit; uses the migration state machine defined in
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/udr_connectors/scratchbird_udr/SPECIFICATION.md
- Potential ambiguity markers detected:
  - L31: - Supported auth methods: password, client certificate, optional MFA challenge.
  - L122: - Migration: optional and explicit; uses the migration state machine defined in
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/wire_protocols/FIREBIRD_EMULATION_BEHAVIOR.md
- Potential ambiguity markers detected:
  - L27: - isc_dpb_auth_plugin_list (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/wire_protocols/MYSQL_EMULATION_BEHAVIOR.md
- Potential ambiguity markers detected:
  - L63: - Warnings should be accessible via SHOW WARNINGS.
  - L91: - Capability flags differ; plugin list may include ed25519.
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/wire_protocols/POSTGRESQL_EMULATION_BEHAVIOR.md
- Potential ambiguity markers detected:
  - L33: - Unknown parameters are accepted and ignored unless explicitly configured.
  - L100: - SCRAM-SHA-256 may be unavailable; default to MD5.
  - L104: - `NegotiateProtocolVersion` may be sent if unknown parameters are rejected
  - L105: (optional; default behavior is to ignore unknowns).
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/wire_protocols/firebird_wire_protocol.md
- Potential ambiguity markers detected:
  - L65: Large logical packets may be sent in partial mode; in that case, the receiver
  - L66: may observe `op_partial` and must continue reading until a complete packet is
  - L330: - Stream may end mid-header; the remainder carries to the next `op_batch_blob_stream`.
  - L425: Servers may respond with:
  - L568: - If an unknown charset/collation is requested, return the appropriate Firebird
  - L678: - Protocol 10 clients may only use legacy authentication (no `op_cont_auth`).
  - L739: 8. Then the same fields as `op_exec_immediate` (transaction, statement, dialect, SQL, items, buffer length, optional flags)
  - L850: 4. Optional fields (timeout/cursor/inline) as above
  - L854: 2. `p_sqldata_blr` (xdr_sql_blr) — output format; may be empty to reuse cached format
  - L1177: The EPB is the buffer built by `isc_event_block` and should be treated as an
  - L1189: client computes deltas using `isc_event_counts` and should re-arm the event
  - L1228: - Optional message parameters use `isc_arg_string` / `isc_arg_cstring`.
  - L1295: input, the server may send the uncompressed payload with
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/wire_protocols/mysql_wire_protocol.md
- Potential ambiguity markers detected:
  - L104: // word may appear after the 10-byte reserved block.
  - L331: Caching SHA-256 authentication is plugin-driven and may require multiple rounds:
  - L369: If the server sends plugin data starting with `0x00`, `0xFE`, or `0xFF`, it may prepend `0x01` to escape it. Clients must ignore the optional leading `0x01` when present.
  - L436: char   database[];            // Null-terminated (optional)
  - L437: char   auth_plugin_name[];    // Null-terminated (optional)
  - L438: lenenc attrs_length;          // Optional attributes
  - L445: - Server may respond with `AuthSwitchRequest` / `AuthMoreData` during re-auth.
  - L615: EOF packets are only used when `CLIENT_DEPRECATE_EOF` is **not** set. An EOF packet is identified by `header == 0xFE` **and** payload length `< 9` bytes (to disambiguate from OK packets that may also use 0xFE).
  - L632: (binary protocol) is enabled, the server may return multiple result sets:
  - L640: For `CLIENT_MULTI_STATEMENTS`, a single `COM_QUERY` may produce multiple
  - L798: - Large payloads may be split into multiple compressed packets; each compressed
  - L801: - If decompression fails, the server should return an error and close the
  - L831: Alpha note: replication is deferred; these commands may return a clear error
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/wire_protocols/postgresql_wire_protocol.md
- Potential ambiguity markers detected:
  - L45: ### 0. SSL/GSSENC Requests (Optional, No Type Byte)
  - L155: If the server does not understand some startup parameters, it may send `NegotiateProtocolVersion` (type 'v') before proceeding.
  - L541: int32 param_types[];   // OIDs of parameter types (0 = unspecified)
  - L659: |-------- Describe ('D') ------>|  (optional)
  - L740: - Describe before execute: if a statement has not been executed yet, ScratchBird may return NoData because result columns are unknown. After first execute, result columns are cached for subsequent Describe calls.
  - L867: - Optional: ERR_DETAIL, ERR_HINT
  - L869: Severity strings used by ScratchBird: ERROR, FATAL, PANIC, WARNING, NOTICE. Other fields may be added as catalog/context mapping expands.
  - L1000: ### Write-after Log (WAL) Data Messages (optional extension)
  - L1002: **Scope Note:** ScratchBird does not implement write-after log (WAL, optional extension)-based replication. This section is included as PostgreSQL protocol reference only.
  - L1015: int64 wal_start;   // WAL (optional extension) start position
  - L1016: int64 wal_end;     // WAL (optional extension) end position
  - L1018: char  wal_data[];  // WAL (optional extension) data
  - L1024: int64 wal_end;     // Current end of WAL (optional extension)
  - L1032: int64 received;    // Last WAL (optional extension) byte received
  - L1033: int64 flushed;     // Last WAL (optional extension) byte flushed
  - L1034: int64 applied;     // Last WAL (optional extension) byte applied
  - L1049: │  SSL Nego  │ (optional)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/wire_protocols/scratchbird_native_wire_protocol.md
- Potential ambiguity markers detected:
  - L54: - **SBLR Bytecode** - Optional compiled query transmission
  - L65: | Compression | zstd (optional, negotiated) |
  - L71: While ScratchBird supports PostgreSQL, MySQL, and Firebird protocols for compatibility (TDS is optional extension), the native protocol provides:
  - L125: │          │ Optional MFA challenge
  - L293: - A logical message may be split across multiple frames.
  - L356: // Optional: application_name, client_encoding, search_path, etc.
  - L820: uint64_t stream_length; // Total byte length (0 if unknown)
  - L959: ### 8.5 QUERY_PLAN Message (Optional)
  - L980: ### 8.6 SBLR_COMPILED Message (Optional)
  - L1187: uint8_t sblr_bytecode[];  // Optional
  - L1350: │  [Optional write-after log (optional extension)]                      │
  - L1548: uint64_t total_rows;     // 0 if unknown
  - L1601: char     filter[];        // Optional filter expression
  - L1647: - **SUB_TYPE_TABLE**: payload is optional; change_type/row_id fields are present.
  - L1648: - **SUB_TYPE_QUERY**: payload is optional; content is client-defined.
  - L1657: uint8_t  message[];        // Optional message (native clients only)
  - L1716: // 'D' = Detail (optional secondary message)
  - L1717: // 'H' = Hint (optional suggestion)
  - L2220: 2. zstd library (optional, for compression)
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/wire_protocols/tds_wire_protocol.md
- Potential ambiguity markers detected:
  - L207: #define UNKNOWN_COLLATION   0x20  // Unknown collation
  - L404: // Parameter 2: Parameter definition (optional)
  - L405: // Parameter 3+: Parameter values (optional)
  - L833: │ TLS (optional)
  - L936: // Unknown token
- Needed clarification: Confirm whether these phrases indicate normative requirements or legacy notes; specify exact behavior and edge cases.

### /docs/specifications/parser/v3/ACCESS_CONTROL.md
- "database-level admin role" is referenced but not defined (role name, privileges, or catalog source unclear).
- SBLR emission requires "string_id" references, but the spec does not define how string_id is allocated or whether it is from the V3 constant pool.
- Error code selection between SQLSTATE 0A000 vs 42809 for invalid privilege/object combos is said to be "context dependent" but the context rules are not specified.
- Grantee resolution order (ROLE, GROUP, USER) lacks a rule for quoted identifiers and mixed-case collisions.
- Security catalog tables require SBDB$ domains, but the SBDB$ domain definitions and expected column domains are not listed in this spec.

### /docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md
- No ambiguity found.

### /docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md
- Spec mixes tests/examples with requirements but does not always define normative defaults (e.g., JSON behavior and "should error" cases lack required SQLSTATE).
- Feature sections include effort/priority but do not define version gates or feature flags for incomplete items.
- When SQL standard is ambiguous (e.g., NULL handling in UNIQUE), defaults are stated but configurability and catalog storage are not specified.

### /docs/specifications/parser/v3/DDL_ALTER.md
- ALTER actions list options like SET OPTION, SET STORAGE PARAMETERS, SET STATISTICS without defining valid option names, types, or ranges.
- Partition bounds for ATTACH/DETACH are not specified (format, inclusive/exclusive, value types).
- Identifier canonicalization references V3 naming rules but does not define them in this spec.

### /docs/specifications/parser/v3/DDL_CREATE.md
- CREATE modifiers (TEMPORARY/UNLOGGED/MATERIALIZED) list syntax but do not define semantic impact or catalog flags.
- OR ALTER / OR REPLACE applicability across object types is not explicitly enumerated.
- CREATE FOREIGN DATA WRAPPER is mentioned as rejected, but error codes are not specified.

### /docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md
- DROP IF EXISTS and CASCADE/RESTRICT semantics are not defined per object type (error code behavior unclear).
- TRUNCATE with multiple tables does not specify lock ordering or FK interaction.

### /docs/specifications/parser/v3/DELETE.md
- USING clause semantics are described but do not define ambiguity resolution when both USING and FROM alias collide.
- No error code guidance for unsupported WHERE CURRENT OF.

### /docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md
- Row lock ordering refers to "primary key or stable UUID" but does not define stable UUID when no PK exists.
- Lock modes list IS/IX but do not define compatibility matrix or escalation rules.
- GC rules reference Firebird sources but do not define ScratchBird-specific page or version chain edge cases.

### /docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md
- Executor rules are high-level and defer to other specs; stack model (value types, memory ownership, error propagation) is not defined here.
- Resource limits (stack depth, recursion, memory) and deterministic error behavior are not specified.

### /docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md
- Spec does not define required join algorithms, plan selection rules, or cost model boundaries (optimizer determinism unclear).
- Behavior for NULL ordering, collation, and type coercion in execution is not explicitly listed (relies on other docs but not cross-referenced).

### /docs/specifications/parser/v3/IMPLEMENTATION_SAFETY_SUMMARY.md
- Checklist references many specs but does not state conflict resolution priority when those specs disagree beyond "V3 tree is authoritative".
- No explicit enforcement mechanism for the checklist is defined.

### /docs/specifications/parser/v3/INSERT.md
- ON CONFLICT behavior references EXCLUDED but does not define resolution when column list is omitted.
- DEFAULT VALUES and DEFAULT in VALUES are noted but do not specify SQLSTATE for invalid default usage.

### /docs/specifications/parser/v3/JOINS.md
- LATERAL support is limited to subqueries but the spec does not state error behavior for LATERAL on table/function refs.
- JOIN USING column coalescing is not defined in detail (column naming/duplication rules).

### /docs/specifications/parser/v3/MERGE.md
- Spec allows NOT MATCHED BY SOURCE but does not define conflict resolution when both BY SOURCE and BY TARGET clauses exist.
- INSERT branch requires VALUES but does not define whether DEFAULT VALUES or SELECT is allowed.

### /docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md
- Operator precedence list does not specify relative precedence of JSON operators or custom operators mentioned elsewhere.
- Set operator precedence does not define parenthesized association rules beyond left-associative.
