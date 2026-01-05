# Internal Native Functions (Actual Implementation)

Purpose: Code-verified inventory of native/built-in function execution in the current codebase, including return types, invocation paths, and V2 parser exposure.

Status: static code review snapshot; no runtime execution performed.

## If you only remember 5 things
- Functions execute through SBLR opcodes in `Executor::evaluateExpression` (plus a few extended opcodes handled in the statement loop); V2 exposes only a narrow subset.
- V2 semantic analysis recognizes more functions than the bytecode generator emits (LTRIM/RTRIM/CONCAT/CONCAT_WS/CURRENT_TIME), and catalog-resolved UDFs are not encoded into bytecode at all.
- Temporal functions return INT64 seconds (not TIMESTAMP/DATE types), and AT TIME ZONE returns a formatted string; units do not match TypedValue timestamp microseconds.
- Array and JSON functions mostly operate on JSON strings; ARRAY_AGG returns JSON, while scalar stats functions expect DataType::ARRAY values.
- Expression index evaluation supports only LOWER, UPPER, LENGTH/LEN, ABS, ROUND; everything else raises errors.

## Scope and sources (code)
- Executor and function dispatch: `ScratchBird/src/sblr/executor.cpp`
- Opcode definitions: `ScratchBird/include/scratchbird/sblr/opcodes.h`
- V2 function resolution: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp`
- V2 bytecode emission: `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Expression index evaluator: `ScratchBird/src/sblr/expression_evaluator.cpp`
- Text search helpers: `ScratchBird/src/core/ts_functions.cpp`
- Value constructors and types: `ScratchBird/src/core/typed_value.cpp`

## Execution pipeline (actual)
- V2 semantic analysis resolves function names, sets return types for known built-ins, and flags aggregates; unknown functions are still marked builtin with UNKNOWN return and a warning.
- Bytecode generation maps built-ins to opcodes/extended opcodes; unknown functions only write the function name string and argument count (no opcode), which the executor does not interpret as a function call.
- Runtime expression evaluation is driven by `Executor::evaluateExpression` and an extended opcode switch; aggregates are executed via AGG_INIT/ACCUMULATE/FINALIZE with `AggregateAccumulator`, with a limited scalar-aggregate fallback that scans the current table.
- Stored functions and procedures use statement-level opcodes (`EXT_FUNCTION`, `EXT_CALL`) and are not emitted by the V2 expression generator.
- Expression index evaluation uses `ExpressionEvaluator`, which is separate and supports only a small function subset.

## V2 surface map (code-verified)

### Functions emitted by BytecodeGeneratorV2
- Aggregates: COUNT, SUM, AVG, MIN, MAX, STDDEV/STDDEV_SAMP, STDDEV_POP, VARIANCE/VAR_SAMP, VAR_POP, CORR, COVAR_POP, ARRAY_AGG.
- String: LENGTH/LEN, UPPER, LOWER, TRIM, SUBSTRING/SUBSTR, CHAR_LENGTH, OCTET_LENGTH, CONVERT, COLLATE.
- Temporal: NOW/CURRENT_TIMESTAMP, CURRENT_DATE, DATE_ADD, DATE_SUB, DATE_DIFF/DATEDIFF.
- Math: ABS, SIGN, ROUND, FLOOR, CEIL/CEILING, TRUNC, MOD, SQRT, CBRT, POWER/POW, EXP, LN, LOG, LOG10, LOG2, SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, DEGREES, RADIANS, PI, SINH, COSH, TANH, ASINH, ACOSH, ATANH, COT.
- JSON/JSONB: JSON_EXTRACT, JSON_OBJECT, JSON_ARRAY, JSON_SET, JSON_INSERT, JSON_REMOVE, JSONB_EXTRACT_PATH, JSONB_BUILD_OBJECT, JSONB_BUILD_ARRAY, JSONB_SET.
- Null handling: COALESCE, NULLIF.
- JSON operators (->, ->>, #>, #>>) are emitted via BinaryOp mapping.

### Recognized by SemanticAnalyzerV2 but not emitted by BytecodeGeneratorV2
- LTRIM, RTRIM.
- CONCAT, CONCAT_WS.
- CURRENT_TIME.

### Implemented in Executor but not surfaced by V2
- Array helpers and operators: ARRAY_TO_STRING, STRING_TO_ARRAY, ARRAY_APPEND/PREPEND/CAT/REMOVE/REPLACE, ARRAY_LENGTH/DIMS/LOWER/UPPER, array containment/overlap.
- Text search: TO_TSVECTOR, TO_TSQUERY, PLAINTO_TSQUERY, PHRASETO_TSQUERY, @@, TS_RANK.
- Regex and string utilities: REGEXP_MATCHES/REPLACE/SPLIT_TO_ARRAY/SPLIT_TO_TABLE, regex operators (~, ~*, !~, !~*), SPLIT_PART, STRPOS, POSITION, OVERLAY, QUOTE_LITERAL/IDENT, INITCAP, ASCII, CHR, REPEAT, REVERSE, LPAD, RPAD.
- Bit/byte utilities: GET_BYTE/SET_BYTE, GET_BIT/SET_BIT, BIT_AND/OR/XOR/NOT, BIT_SHIFT_*, BIT_COUNT, BIT_LENGTH, BIT_MASK.
- Crypto/encoding: MD5, SHA1, SHA256, SHA512, ENCODE, DECODE.
- XML: XMLPARSE, XMLSERIALIZE, XMLELEMENT, XMLCONCAT, XMLFOREST, XMLCOMMENT, XMLROOT, XPATH, XMLEXISTS.
- Spatial: ST_* constructors/accessors/geometry ops (see below), plus ST_SRID/SET_SRID/TRANSFORM/DISTANCE_SPHERE handled in the statement loop.
- EXTRACT(field FROM value).
- Scalar stats on arrays: STDDEV_SAMP/POP, VAR_SAMP/POP, CORR, COVAR_POP.
- GROUPING(), NULL-safe equality, LIKE/ILIKE with ESCAPE, IN value-list operator, placeholders, and domain-pipeline opcodes.

## Function reference (actual implementation)

### Aggregate functions (AGG_* opcodes)
| Function | Opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| COUNT | AGG_COUNT | 1 | INT64 | Counts all rows; NULLs skipped only for COUNT(expr) | Y |
| SUM | AGG_SUM | 1 | FLOAT64 | Uses val.toDouble() | Y |
| AVG | AGG_AVG | 1 | FLOAT64 | Uses val.toDouble() | Y |
| MIN | AGG_MIN | 1 | Value | Comparison uses toDouble; returns first typed Value | Y |
| MAX | AGG_MAX | 1 | Value | Comparison uses toDouble; returns first typed Value | Y |
| ARRAY_AGG | ARRAY_AGG | 1 | JSON | Returns JSON array string, not DataType::ARRAY | Y |
| STDDEV_SAMP | AGG_STDDEV_SAMP | 1 | FLOAT64 | Welford; requires n>1 | Y |
| STDDEV_POP | AGG_STDDEV_POP | 1 | FLOAT64 | Welford; requires n>0 | Y |
| VAR_SAMP | AGG_VAR_SAMP | 1 | FLOAT64 | Welford; requires n>1 | Y |
| VAR_POP | AGG_VAR_POP | 1 | FLOAT64 | Welford; requires n>0 | Y |
| CORR | AGG_CORR | 2 | FLOAT64 | Two-arg aggregate; requires n>1 | Y |
| COVAR_POP | AGG_COVAR_POP | 2 | FLOAT64 | Two-arg aggregate; requires n>0 | Y |
| REGR_* | AGG_REGR_* | 2 | FLOAT64/INT64 | Implemented in accumulator finalize; not emitted by V2 | N |

Notes: Scalar aggregate fallback only supports COUNT/SUM/AVG/MIN/MAX/ARRAY_AGG and scans the current table. DISTINCT tracking exists in the accumulator but V2 bytecode does not emit DISTINCT flags.

### String functions (Opcode)
| Function | Opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| LENGTH/LEN | FUNC_LENGTH | 1 | INT32 | UTF-8 char count; NULL -> NULL | Y |
| SUBSTRING/SUBSTR | FUNC_SUBSTRING | 3 | VARCHAR | 1-based; <=0 length yields empty string | Y |
| UPPER | FUNC_UPPER | 1 | VARCHAR | UTF-8 aware case mapping | Y |
| LOWER | FUNC_LOWER | 1 | VARCHAR | UTF-8 aware case mapping | Y |
| TRIM | FUNC_TRIM | 1 | VARCHAR | Whitespace trim only; no LTRIM/RTRIM modes | Y |
| CHAR_LENGTH | FUNC_CHAR_LENGTH | 1 | INT32 | Same as LENGTH | Y |
| OCTET_LENGTH | FUNC_OCTET_LENGTH | 1 | INT32 | Byte length | Y |
| CONVERT | FUNC_CONVERT | 3 | VARCHAR | Charset IDs are numeric (core::CharacterSet) | Y |
| COLLATE | FUNC_COLLATE | 2 | Value | No-op; collation metadata not attached | Y |

### Array <-> string (Opcode)
| Function | Opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| ARRAY_TO_STRING | ARRAY_TO_STRING | 2-3 | TEXT | Parses JSON array string; NULL/invalid -> NULL | N |
| STRING_TO_ARRAY | STRING_TO_ARRAY | 2-3 | JSON | Splits string; empty delimiter splits bytes | N |

### Temporal functions (Opcode)
| Function | Opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| DATE_ADD | FUNC_DATE_ADD | 2 | INT64 | Treats input as seconds; adds days*86400 | Y |
| DATE_SUB | FUNC_DATE_SUB | 2 | INT64 | Treats input as seconds; subtracts days*86400 | Y |
| DATE_DIFF | FUNC_DATE_DIFF | 2 | INT64 | (t1 - t2)/86400 | Y |
| NOW/CURRENT_TIMESTAMP | FUNC_NOW | 0 | INT64 | Seconds since epoch; not TIMESTAMP | Y |
| CURRENT_DATE | FUNC_CURRENT_DATE | 0 | INT64 | Midnight seconds since epoch; not DATE | Y |
| AT TIME ZONE | FUNC_AT_TIME_ZONE | 2 | VARCHAR | Uses timezone_manager_.formatTimestamp | N |

### JSON/JSONB functions (Opcode)
| Function | Opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| JSON_EXTRACT / -> | JSON_EXTRACT / JSON_ARROW | 2 | JSON | parseJSONPath; NULL/invalid -> NULL | Y |
| ->> | JSON_DOUBLE_ARROW | 2 | TEXT | text extraction | Y (operator) |
| JSONB_EXTRACT_PATH | JSONB_EXTRACT_PATH | >=2 | JSON | path elements as args | Y |
| #> / #>> | JSON_HASH_ARROW / JSON_HASH_DOUBLE_ARROW | 2 | JSON/TEXT | path array string "{a,b}" | Y (operator) |
| JSON_OBJECT / JSONB_BUILD_OBJECT | JSON_OBJECT / JSONB_BUILD_OBJECT | even | JSON | key/value pairs; error if odd | Y |
| JSON_ARRAY / JSONB_BUILD_ARRAY | JSON_ARRAY / JSONB_BUILD_ARRAY | n | JSON | returns JSON array | Y |
| JSON_SET / JSONB_SET | JSON_SET / JSONB_SET | 3 | JSON | path set; invalid JSON returns original | Y |
| JSON_INSERT | JSON_INSERT | 3 | JSON | inserts only if path missing | Y |
| JSON_REMOVE | JSON_REMOVE | 2 | JSON | removes path if present | Y |

Notes: JSONB_* functions return DataType::JSON (string) and do not implement binary JSONB semantics.

### Array functions (ExtendedOpcode, JSON-string based)
| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| ARRAY_APPEND | EXT_ARRAY_APPEND | 2 | JSON | non-array -> empty array; invalid JSON -> NULL | N |
| ARRAY_PREPEND | EXT_ARRAY_PREPEND | 2 | JSON | same behavior as APPEND | N |
| ARRAY_CAT | EXT_ARRAY_CAT | 2 | JSON | concatenates JSON arrays | N |
| ARRAY_REMOVE | EXT_ARRAY_REMOVE | 2 | JSON | removes matching JSON value; non-array -> NULL | N |
| ARRAY_REPLACE | EXT_ARRAY_REPLACE | 3 | JSON | replaces matches; non-array -> NULL | N |
| && | EXT_ARRAY_OVERLAP | 2 | BOOLEAN | invalid JSON -> false | N |
| @> | EXT_ARRAY_CONTAINS | 2 | BOOLEAN | invalid JSON -> false | N |
| <@ | EXT_ARRAY_CONTAINED_BY | 2 | BOOLEAN | invalid JSON -> false | N |
| ARRAY_LENGTH | EXT_ARRAY_LENGTH | 1-2 | INT64 | ignores dimension; 1D only | N |
| ARRAY_DIMS | EXT_ARRAY_DIMS | 1 | TEXT | returns "[1:n]" | N |
| ARRAY_UPPER | EXT_ARRAY_UPPER | 1-2 | INT64 | upper bound = size | N |
| ARRAY_LOWER | EXT_ARRAY_LOWER | 1-2 | INT64 | lower bound = 1 | N |
| ARRAY_CONSTRUCT | EXT_ARRAY_CONSTRUCT | n | JSON | valueToJSON per element | N |

Notes: These functions expect arrays as JSON strings, not DataType::ARRAY.

### Scalar statistics on arrays (ExtendedOpcode)
| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| STDDEV_SAMP | EXT_STDDEV_SAMP | 1 | FLOAT64 | DataType::ARRAY only; n>1 | N |
| STDDEV_POP | EXT_STDDEV_POP | 1 | FLOAT64 | DataType::ARRAY only; n>0 | N |
| VAR_SAMP | EXT_VAR_SAMP | 1 | FLOAT64 | DataType::ARRAY only; n>1 | N |
| VAR_POP | EXT_VAR_POP | 1 | FLOAT64 | DataType::ARRAY only; n>0 | N |
| CORR | EXT_CORR | 2 | FLOAT64 | Two arrays, equal length, n>1 | N |
| COVAR_POP | EXT_COVAR_POP | 2 | FLOAT64 | Two arrays, equal length, n>0 | N |

### Text search (ExtendedOpcode)
| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| TO_TSVECTOR | EXT_TO_TSVECTOR | 1-2 | TSVECTOR | optional config; null if config missing | N |
| TO_TSQUERY | EXT_TO_TSQUERY | 1-2 | TSQUERY | optional config | N |
| PLAINTO_TSQUERY | EXT_PLAINTO_TSQUERY | 1-2 | TSQUERY | tokenized AND query | N |
| PHRASETO_TSQUERY | EXT_PHRASETO_TSQUERY | 1-2 | TSQUERY | phrase query | N |
| @@ | EXT_TSMATCH | 2 | BOOLEAN | supports TSVECTOR/TSQUERY and TEXT/TSQUERY | N |
| TS_RANK | EXT_TS_RANK | 2-3 | FLOAT64 | TSVECTOR + TSQUERY only | N |

### Regex and string utilities (ExtendedOpcode)
| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| ~, ~*, !~, !~* | EXT_REGEX_MATCH/_CI/_NOT/_NOT_CI | 2 | BOOLEAN | std::regex ECMAScript; invalid regex -> error | N |
| REGEXP_MATCHES | EXT_REGEXP_MATCHES | 2-3 | JSON | returns JSON array of matches | N |
| REGEXP_REPLACE | EXT_REGEXP_REPLACE | 3-4 | TEXT | flags support i/g | N |
| REGEXP_SPLIT_TO_ARRAY | EXT_REGEXP_SPLIT_TO_ARRAY | 2-3 | JSON | returns JSON array | N |
| REGEXP_SPLIT_TO_TABLE | EXT_REGEXP_SPLIT_TO_TABLE | 2-3 | JSON | returns array; table expansion not implemented | N |
| STRING_TO_TABLE | EXT_STRING_TO_TABLE | 2 | JSON | table expansion not implemented | N |
| UNNEST_TEXT | EXT_UNNEST_TEXT | 1 | JSON | passes through JSON array or wraps value | N |
| SPLIT_PART | EXT_SPLIT_PART | 3 | TEXT | 1-based field; out of range -> "" | N |
| STRPOS | EXT_STRPOS | 2 | INT32 | 1-based position; 0 if not found | N |
| POSITION | EXT_POSITION | 2 | INT32 | args reversed (substring, text) | N |
| OVERLAY | EXT_OVERLAY | 3-4 | TEXT | replace segment; duplicate implementation exists | N |
| QUOTE_LITERAL | EXT_QUOTE_LITERAL | 1 | TEXT | NULL -> "NULL" | N |
| QUOTE_IDENT | EXT_QUOTE_IDENT | 1 | TEXT | NULL -> NULL | N |
| INITCAP | EXT_INITCAP | 1 | TEXT | ASCII word casing | N |
| ASCII | EXT_ASCII | 1 | INT32 | empty string -> 0 | N |
| CHR | EXT_CHR | 1 | TEXT | accepts 0-255 | N |
| REPEAT | EXT_REPEAT | 2 | TEXT | negative count -> "" | N |
| REVERSE | EXT_REVERSE | 1 | TEXT | byte-wise reverse | N |
| LPAD/RPAD | EXT_LPAD/EXT_RPAD | 2-3 | TEXT | pad or truncate; fill default " " | N |

### Bit and byte utilities (ExtendedOpcode)
| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| GET_BYTE | EXT_GET_BYTE | 2 | INT64 | bytes from toString; 0-based | N |
| SET_BYTE | EXT_SET_BYTE | 3 | VARCHAR | returns mutated string | N |
| GET_BIT | EXT_GET_BIT | 2 | INT64 | bit offset uses MSB first | N |
| SET_BIT | EXT_SET_BIT | 3 | VARCHAR | returns mutated string | N |
| BIT_AND/OR/XOR | EXT_BIT_AND/OR/XOR | 2 | INT64 | integer bitwise ops | N |
| BIT_NOT | EXT_BIT_NOT | 1 | INT64 | bitwise complement | N |
| BIT_SHIFT_LEFT/RIGHT/RIGHT_LOGICAL | EXT_BIT_SHIFT_* | 2 | INT64 | right_logical uses uint64 | N |
| BIT_COUNT | EXT_BIT_COUNT | 1 | INT32 | popcount | N |
| BIT_LENGTH | EXT_BIT_LENGTH | 1 | INT32 | bytes length * 8 | N |
| BIT_MASK | EXT_BIT_MASK | 1 | INT64 | length 0-64 | N |

### Cryptographic and encoding (ExtendedOpcode)
| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| MD5 | EXT_MD5 | 1 | VARCHAR | hex string | N |
| SHA1 | EXT_SHA1 | 1 | VARCHAR | hex string | N |
| SHA256 | EXT_SHA256 | 1 | VARCHAR | hex string | N |
| SHA512 | EXT_SHA512 | 1 | VARCHAR | hex string | N |
| ENCODE | EXT_ENCODE | 2 | TEXT | formats: hex/base64/escape | N |
| DECODE | EXT_DECODE | 2 | UUID | returns DataType::UUID with binary_data_ | N |

### XML (ExtendedOpcode)
| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| XMLPARSE | EXT_XMLPARSE | 2 | VARCHAR | uses libxml2 if available | N |
| XMLSERIALIZE | EXT_XMLSERIALIZE | 3 | VARCHAR | ignores type; returns string | N |
| XMLELEMENT | EXT_XMLELEMENT | 2 | VARCHAR | simple <name>content</name> | N |
| XMLCONCAT | EXT_XMLCONCAT | n | VARCHAR | concatenates XML strings | N |
| XMLFOREST | EXT_XMLFOREST | n | VARCHAR | name/value pairs | N |
| XMLCOMMENT | EXT_XMLCOMMENT | 1 | VARCHAR | rejects "--" | N |
| XMLROOT | EXT_XMLROOT | 3 | VARCHAR | builds XML declaration | N |
| XPATH | EXT_XPATH | 2 | VARCHAR | returns JSON array string; requires libxml2 | N |
| XMLEXISTS | EXT_XMLEXISTS | 2 | BOOLEAN | requires libxml2 | N |

Notes: XML opcodes are duplicated in the executor with inconsistent arg-count checks; the first match in the chain enforces the counts above.

### Spatial (ExtendedOpcode)
Constructors and accessors are in `evaluateExpression`; SRID/transform functions are handled in the statement loop.

| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| ST_Point | EXT_ST_POINT | 2 | POINT | x,y numeric | N |
| ST_MakeLine | EXT_ST_MAKELINE | >=2 | LINESTRING | points only | N |
| ST_MakePolygon | EXT_ST_MAKEPOLYGON | 1 | POLYGON | linestring only | N |
| ST_AsText | EXT_ST_ASTEXT | 1 | TEXT | WKT output | N |
| ST_AsBinary | EXT_ST_ASBINARY | 1 | TEXT | WKB bytes in TEXT | N |
| ST_GeometryType | EXT_ST_GEOMETRYTYPE | 1 | TEXT | type name | N |
| ST_IsValid | EXT_ST_ISVALID | 1 | BOOLEAN | supports POINT/LINESTRING/POLYGON only | N |
| ST_NumGeometries | EXT_ST_NUMGEOMETRIES | 1 | INT32 | simple geoms return 1 | N |
| ST_GeometryN | EXT_ST_GEOMETRYN | 2 | geometry | 1-based index | N |
| ST_MultiPoint | EXT_ST_MULTIPOINT | >=1 | MULTIPOINT | points only | N |
| ST_MultiLineString | EXT_ST_MULTILINESTRING | >=1 | MULTILINESTRING | linestrings only | N |
| ST_MultiPolygon | EXT_ST_MULTIPOLYGON | >=1 | MULTIPOLYGON | polygons only | N |
| ST_GeometryCollection/ST_Collect | EXT_ST_GEOMETRYCOLLECTION / EXT_ST_COLLECT | >=1 | GEOMETRYCOLLECTION | skips NULLs | N |

GEOS-backed ops (require HAVE_GEOS):
- ST_Buffer, ST_ConvexHull, ST_Envelope, ST_Intersects, ST_Contains, ST_Within, ST_Equals, ST_Disjoint, ST_Overlaps, ST_Touches, ST_Crosses, ST_Intersection, ST_Union, ST_Difference, ST_Area, ST_Length, ST_Distance, ST_Perimeter.

Statement-loop spatial ops:
- ST_SRID, ST_SetSRID, ST_Transform (requires PROJ), ST_Distance_Sphere (Haversine, POINT only).

### Metadata/introspection (ExtendedOpcode)
| Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| FORMAT_TYPE | EXT_FUNC_FORMAT_TYPE | 1-2 | TEXT | maps PG OIDs + domain hash | Y |
| OBJ_DESCRIPTION | EXT_FUNC_OBJ_DESCRIPTION | 1-2 | TEXT | comments by catalog | Y |
| SHOBJ_DESCRIPTION | EXT_FUNC_SHOBJ_DESCRIPTION | 1-2 | TEXT | same handler as OBJ_DESCRIPTION | Y |
| COL_DESCRIPTION | EXT_FUNC_COL_DESCRIPTION | 2 | TEXT | column comment by rel_oid/attnum | Y |
| AGE | EXT_FUNC_AGE | 1-2 | INTERVAL | months=0; days + microseconds only | N |

### EXTRACT(field FROM value) (ExtendedOpcode)
- DATE fields: YEAR, MONTH, DAY, DOW, DOY, QUARTER, EPOCH.
- TIME fields: HOUR, MINUTE, SECOND, MICROSECOND, MILLISECOND, EPOCH.
- TIMESTAMP fields: YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, MICROSECOND, MILLISECOND, DOW, DOY, QUARTER, EPOCH.
- INTERVAL fields: YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, MICROSECOND, MILLISECOND, EPOCH (approx, 30 days/month).
- UUID fields: VERSION, VARIANT, TIMESTAMP (v1/v7), CLOCK_SEQ (v1), NODE (v1 MAC string).
- ARRAY fields: CARDINALITY, NDIMS (1), LOWER (1), UPPER, DIMS (returns ARRAY of dimension sizes).
- POINT fields: X, Y, SRID.
- Unsupported type/field combinations raise errors.

### Special forms and operators (ExtendedOpcode)
| Operator/Function | Ext opcode | Args | Return | Notes | V2 |
| --- | --- | --- | --- | --- | --- |
| NULL-safe equality | EXT_NULL_SAFE_EQ | 2 | BOOLEAN | NULL==NULL true | N |
| LIKE/ILIKE ESCAPE | EXT_LIKE_ESCAPE/EXT_ILIKE_ESCAPE | 3 | BOOLEAN | ESCAPE must be single char | N |
| IN (value list) | EXT_IN_LIST | 2 | BOOLEAN/NULL | list is JSON array; NULL in -> NULL | N |
| GROUPING | EXT_GROUPING_FUNC | 1 | INT32 | heuristic for grouping sets | N |
| Placeholder | EXT_PLACEHOLDER | 0 | Value | resolves bound param | N |

### Domain pipeline (internal extended opcodes)
| Opcode | Args | Return | Notes |
| --- | --- | --- | --- |
| EXT_CHECK_DOMAIN_CONSTRAINT | domain_id, value_offset | BOOLEAN | false on constraint violation |
| EXT_APPLY_DOMAIN_MASKING | domain_id, user_id, value_offset | side-effect | replaces value on stack |
| EXT_ENCRYPT_DOMAIN_VALUE | domain_id, value_offset | side-effect | encrypts value on stack |
| EXT_DECRYPT_DOMAIN_VALUE | domain_id, value_offset | side-effect | decrypts value on stack |
| EXT_AUDIT_DOMAIN_ACCESS | domain_id, user_id, table_id, column_id | side-effect | logs audit event |
| EXT_CHECK_DOMAIN_PRIVILEGE | domain_id, user_id | BOOLEAN | masking privilege check |
| EXT_NORMALIZE_DOMAIN_VALUE | domain_id, value_offset | side-effect | normalization pipeline |
| EXT_VALIDATE_DOMAIN_VALUE | domain_id, value_offset | BOOLEAN | true/false validity |
| EXT_APPLY_QUALITY_PIPELINE | domain_id, value_offset | side-effect | quality pipeline |
| EXT_CHECK_GLOBAL_UNIQUENESS | domain_id, table_id, column_id, row_id, value_offset | BOOLEAN | global uniqueness check |

### Stored functions/procedures (statement-level)
- EXT_FUNCTION executes stored function bytecode by name with permission checks and SQL SECURITY context.
- EXT_CALL executes stored procedures; V2 expression generator does not emit these opcodes.

### Expression index evaluator (ExpressionEvaluator)
- Supported functions: LOWER, UPPER, LENGTH/LEN, ABS, ROUND.
- Behavior differs from executor: uses ASCII `tolower`/`toupper`, LENGTH uses byte count.
- Aggregates and EXTRACT are explicitly unsupported.

## Known gaps and inconsistencies (code-verified)
- JSONB_* functions return DataType::JSON and use JSON string parsing; no JSONB binary semantics.
- DATE_* and NOW/CURRENT_DATE return INT64 seconds, while semantic analyzer types them as DATE/TIMESTAMP and TypedValue timestamps use microseconds.
- AT TIME ZONE returns formatted text and is not surfaced by V2.
- ARRAY_AGG returns JSON string, while array statistics functions require DataType::ARRAY values.
- DECODE returns DataType::UUID (binary_data) rather than BYTEA/VARBINARY.
- Duplicate EXT_OVERLAY and XML opcode handlers exist in the executor; only the first match in the chain is reachable.
