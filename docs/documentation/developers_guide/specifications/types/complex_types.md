# Specification: Complex Types

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | types / catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.3.0 |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/types.h:80`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2753`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2761`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/vector.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/converted/core/create_type.sql`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/converted/core/rowtypes.sql`

## Synopsis

This specification defines the canonical complex and structured types supported by ScratchBird, including arrays, JSON, vectors, spatial geometries, composite types, and specialized search types. Complex types extend scalar types with structured storage and specialized operations.

## Scope

### In Scope

- ARRAY - Homogeneous ordered collections
- JSON / JSONB - Document storage with text and binary formats
- XML - Document storage
- VECTOR - Embedding vectors for similarity search
- Spatial types (POINT, LINESTRING, POLYGON, MULTI*, GEOMETRYCOLLECTION, GEOMETRY)
- COMPOSITE / ROW - Record/struct types
- LIST / MAP - Container types
- ENUM / SET - Enumerated types
- Range types (INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, TSTZRANGE, DATERANGE)
- Text search types (TSVECTOR, TSQUERY)
- BSON - MongoDB document format

### Out of Scope

- Scalar types - see [scalar_types.md](./scalar_types.md)
- Type coercion rules - see [type_coercion_rules.md](./type_coercion_rules.md)
- Domain wrapper types - see domain specifications

## Background

Complex types in ScratchBird provide structured data storage with specialized binary formats. Many complex types follow PostgreSQL-compatible binary representations for interoperability. Complex types are generally TOAST-eligible and support specialized indexing (GIN for JSON/JSONB, HNSW for vectors, R-tree for spatial).

## Specification

### DataType Enum - Complex Types

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/types.h:80
enum class DataType : uint16_t
{
    // JSON and document types (60-64)
    UUID = 60,
    JSON = 61,       // Text JSON with validation
    JSONB = 62,      // Binary JSON (PostgreSQL-compatible)
    XML = 63,        // XML document
    VECTOR = 64,     // Vector embeddings
    
    // Spatial types (65-79)
    POINT = 65,
    LINESTRING = 66,
    POLYGON = 67,
    MULTIPOINT = 68,
    MULTILINESTRING = 69,
    MULTIPOLYGON = 70,
    GEOMETRYCOLLECTION = 71,
    GEOMETRY = 72,   // Generic geometry container
    
    // Array and composite types (80-89)
    ARRAY = 80,
    COMPOSITE = 81,
    LIST = 82,
    MAP = 83,
    BSON = 84,       // MongoDB document format
    
    // Text search types (90-91)
    TSVECTOR = 90,
    TSQUERY = 91,
    
    // Range types (92-97)
    INT4RANGE = 92,
    INT8RANGE = 93,
    NUMRANGE = 94,
    TSRANGE = 95,
    TSTZRANGE = 96,
    DATERANGE = 97,
    
    // User-defined types (102-109)
    DOMAIN = 102,
    ROW = 103,
    ENUM = 104,
    SET = 105,
    
    // Polymorphic types (110-119)
    VARIANT = 110,   // Tagged union
};
```

### JSON Types

#### JSON (Text Format)

- Stored as UTF-8 text with `u32` length prefix
- Validation enforced according to `types.json.validation` configuration
- Preserves original formatting and key order

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2753
TypedValue TypedValue::makeJSON(const std::string& value)
{
    TypedValue tv(DataType::JSON);
    tv.is_null_ = false;
    tv.string_data_ = value;
    return tv;
}
```

#### JSONB (Binary Format)

Uses PostgreSQL-compatible binary JSONB format:

| Component | Description |
|-----------|-------------|
| Root container | Array (JB_FARRAY) or Object (JB_FOBJECT) |
| Scalar roots | Use JB_FSCALAR flag |
| JEntry ordering | Follows PostgreSQL alignment rules |
| Storage | CBOR-encoded canonical representation |

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2761
TypedValue TypedValue::makeJSONB(const std::vector<uint8_t>& value)
{
    TypedValue tv(DataType::JSONB);
    tv.is_null_ = false;
    tv.binary_data_ = value;
    return tv;
}
```

**JSONB Encoding:**
```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:467
bool encodeJsonb(const std::string& text, std::vector<uint8_t>& out, ErrorContext* ctx)
{
    try
    {
        Json parsed = Json::parse(text);
        OrderedJson canonical = canonicalizeJson(parsed);
        out = OrderedJson::to_cbor(canonical);
        return true;
    }
    catch (...)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION, "Invalid JSONB text");
        return false;
    }
}
```

### XML Type

- Stored as UTF-8 text with `u32` length prefix
- Validation enforced according to `types.xml.validation` configuration
- Preserves document structure including comments and processing instructions

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2809
TypedValue TypedValue::makeXML(const std::string& value)
{
    TypedValue tv(DataType::XML);
    tv.is_null_ = false;
    tv.string_data_ = value;
    return tv;
}
```

### ARRAY Type

Uses PostgreSQL array binary format:

| Field | Type | Description |
|-------|------|-------------|
| ndim | int32 | Number of dimensions |
| dataoffset | int32 | Offset to data (0 if no null bitmap) |
| elemtype_oid | int32 | Element type OID |
| dims[ndim] | int32[] | Size of each dimension |
| lower_bounds[ndim] | int32[] | Lower bound of each dimension |
| null bitmap | bits | Optional null bitmap (if dataoffset > 0) |
| elements | variable | Element payloads in row-major order |

**Element Storage:**
- Each element uses the canonical storage format of the element type
- Elements are stored contiguously in row-major order

### VECTOR Type

Vector embeddings for similarity search with multiple element types:

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/vector.h
struct VectorHeader
{
    uint16_t element_type;  // FLOAT32, FLOAT16, BFLOAT16, INT8, BINARY
    uint16_t flags;         // bit 0 = sparse, bit 1 = normalized
    uint32_t dimension;     // Vector dimension
};
```

#### Dense Vector Format

| Field | Type | Description |
|-------|------|-------------|
| header | VectorHeader | 8 bytes |
| payload | bytes | Packed elements in little-endian order |

**Element Type Sizes:**

| Element Type | Bytes per Element |
|--------------|-------------------|
| FLOAT32 | 4 |
| FLOAT16 | 2 |
| BFLOAT16 | 2 |
| INT8 | 1 |
| BINARY | 1 bit (packed) |

**BINARY Vector:**
- Uses `ceil(dimension/8)` bytes
- Bits packed with MSB-first ordering

#### Sparse Vector Format

| Field | Type | Description |
|-------|------|-------------|
| header | VectorHeader | 8 bytes (sparse bit set) |
| nnz | uint32 | Number of non-zero elements |
| entries | (index, value)[] | uint32 index + float32 value pairs |

### Spatial Types (Geometry)

All spatial types use WKB (Well-Known Binary) with optional SRID using EWKB-style layout:

| Field | Type | Description |
|-------|------|-------------|
| byte_order | uint8 | 1 = little-endian, 0 = big-endian |
| type | uint32 | Geometry type with SRID flag |
| srid | uint32 | Optional (if SRID flag set) |
| payload | bytes | Geometry-specific data |

#### Geometry Type IDs

| Type | WKB Type ID |
|------|-------------|
| POINT | 1 |
| LINESTRING | 2 |
| POLYGON | 3 |
| MULTIPOINT | 4 |
| MULTILINESTRING | 5 |
| MULTIPOLYGON | 6 |
| GEOMETRYCOLLECTION | 7 |

#### Point

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/types.h:248
struct Point
{
    double x;
    double y;
    int32_t srid;  // Spatial Reference ID (0 = undefined)
};
```

Storage: `int32 srid + double x + double y` (16 bytes)

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2852
TypedValue TypedValue::makePoint(const Point& value)
{
    TypedValue tv(DataType::POINT);
    tv.is_null_ = false;
    tv.spatial_data_ = std::make_unique<SpatialData>();
    tv.spatial_data_->point = value;
    return tv;
}
```

#### LineString

Storage: `uint32 point_count + (Point){point_count}`

#### Polygon

Storage: `uint32 ring_count + for each ring: uint32 point_count + (Point){point_count}`
- First ring is exterior (counter-clockwise)
- Subsequent rings are holes (clockwise)

#### Multi-Geometries

Storage: `uint32 count + (WKBGeometry){count}`

### COMPOSITE / ROW Type

Uses PostgreSQL composite format:

| Field | Type | Description |
|-------|------|-------------|
| field_count | int32 | Number of fields |
| fields | repeated | Field data |

Each field:
| Field | Type | Description |
|-------|------|-------------|
| field_type_oid | int32 | Type OID of field |
| field_len | int32 | Length of field data (-1 = NULL) |
| field_data | bytes | field_len bytes of data |

### LIST Type

Ordered list container (domain-backed semantic type):

| Field | Type | Description |
|-------|------|-------------|
| element_count | uint32 | Number of elements |
| elements | bytes[] | Element payloads in order |

### MAP Type

Key-value container:

| Field | Type | Description |
|-------|------|-------------|
| entry_count | uint32 | Number of entries |
| entries | repeated | Sorted by key (for deterministic encoding) |

Each entry:
| Field | Type | Description |
|-------|------|-------------|
| key | bytes | Key payload |
| value | bytes | Value payload |

### ENUM Type

- Stored as `u16` ordinal (1-based to match MySQL/PostgreSQL wire expectations)
- Label list stored in catalog
- Ordinal 0 is reserved/invalid

### SET Type (MySQL Compatibility)

Stored as either:
- `u64` bitset for up to 64 labels
- Sorted list of `u16` ordinals for >64 labels

The encoding type is recorded in type metadata.

### Range Types

Uses PostgreSQL range binary format:

| Field | Type | Description |
|-------|------|-------------|
| flags | uint8 | Bounds flags |
| lower | variable | Lower bound value (if applicable) |
| upper | variable | Upper bound value (if applicable) |

**Flags:**
- Bit 0: Empty range
- Bit 1: Lower bound inclusive
- Bit 2: Upper bound inclusive
- Bit 3: Lower bound is unbounded
- Bit 4: Upper bound is unbounded

Range type variants:
- INT4RANGE - Range of INT32 values
- INT8RANGE - Range of INT64 values
- NUMRANGE - Range of DECIMAL/FLOAT64 values
- TSRANGE - Range of TIMESTAMP values
- TSTZRANGE - Range of TIMESTAMP WITH ZONE values
- DATERANGE - Range of DATE values

### TSVECTOR (Text Search Vector)

Uses PostgreSQL binary format:

| Field | Type | Description |
|-------|------|-------------|
| lexeme_count | int32 | Number of lexemes |
| lexemes | repeated | Sorted lexeme entries |

Each lexeme:
| Field | Type | Description |
|-------|------|-------------|
| text_length | uint8 | Length of lexeme text |
| text | bytes | Lexeme text |
| position_count | uint16 | Number of positions |
| positions | uint16[] | Word positions with weight flags |

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2953
TypedValue TypedValue::makeTSVector(const TSVector& value)
{
    TypedValue tv(DataType::TSVECTOR);
    tv.is_null_ = false;
    tv.complex_data_ = std::make_unique<ComplexData>();
    tv.complex_data_->tsvector = std::make_shared<TSVector>(value);
    return tv;
}
```

### TSQUERY (Text Search Query)

Uses PostgreSQL binary format for search expressions with operators.

### BSON Type (MongoDB)

- Stored as canonical BSON bytes with `u32` length prefix
- BSON subtypes and extended numeric types preserved in payload
- Compatible with MongoDB wire format

## Interface Contracts

### Type Classification

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/type_system.cpp:419
bool isContainerType(DataType type)
{
    return type == DataType::ARRAY || type == DataType::LIST ||
           type == DataType::COMPOSITE || type == DataType::MAP ||
           type == DataType::VARIANT || type == DataType::SET ||
           type == DataType::ENUM || type == DataType::ROW;
}

bool isGeometryType(DataType type)
{
    return type == DataType::GEOMETRY || type == DataType::POINT ||
           type == DataType::LINESTRING || type == DataType::POLYGON ||
           type == DataType::MULTIPOINT || type == DataType::MULTILINESTRING ||
           type == DataType::MULTIPOLYGON || type == DataType::GEOMETRYCOLLECTION;
}

bool isJsonFamily(DataType type)
{
    return type == DataType::JSON || type == DataType::JSONB ||
           type == DataType::BSON || type == DataType::XML;
}
```

### Object-Like Type Detection

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/src/core/typed_value.cpp:2226
bool isObjectLikeType(DataType type)
{
    if (type == DataType::ARRAY || type == DataType::LIST || type == DataType::MAP ||
        type == DataType::COMPOSITE || type == DataType::ROW ||
        type == DataType::VARIANT)
    {
        return true;
    }
    // ... geometry types also return true
}
```

## Algorithms

### Array Element Access

```
Input:  ARRAY value, indices[]
Output: Element value or NULL

1. Verify indices length matches array dimensions
2. Calculate linear offset: 
   offset = sum(indices[i] - lower_bounds[i]) * stride[i]
3. Check null bitmap at offset (if present)
4. Return element at dataoffset + offset * element_size
```

### Vector Similarity (Cosine)

```
Input:  Vector a, Vector b
Output: Cosine similarity (-1 to 1)

1. Verify dimensions match
2. Compute dot_product = sum(a[i] * b[i])
3. Compute norm_a = sqrt(sum(a[i]^2))
4. Compute norm_b = sqrt(sum(b[i]^2))
5. Return dot_product / (norm_a * norm_b)
```

### Geometry Validation

```
Input:  Geometry value
Output: Valid/Invalid with error details

1. Check type-specific requirements:
   - Point: Always valid
   - LineString: >= 2 points
   - Polygon: >= 1 ring, exterior >= 4 points, all rings closed
   - Multi*: All contained geometries valid
2. Verify SRID consistency (if specified)
3. Return validation result
```

## Invariants

1. **JSONB Canonicalization**: JSONB values are stored in canonical CBOR format
   - Verification: `canonicalizeJson()` before encoding

2. **Vector Dimension Consistency**: Vector operations require matching dimensions
   - Verification: Runtime checks in vector operations

3. **Geometry SRID Consistency**: Multi-geometries require consistent SRIDs
   - Verification: SRID check during geometry construction

4. **Array Bounds**: Array indices validated against dimension bounds
   - Verification: Bounds check before element access

5. **ENUM Ordinal Validity**: ENUM ordinals must be > 0 and <= label count
   - Verification: Catalog lookup on ENUM access

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_TEXT_REPRESENTATION` | Invalid JSON/XML format | Return error to caller |
| `DATA_CORRUPTED` | Invalid binary geometry/vector | Return error, may trigger repair |
| `NUMERIC_VALUE_OUT_OF_RANGE` | Vector dimension mismatch | Return error to caller |
| `INVALID_ARGUMENT` | Invalid geometry construction | Return error to caller |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/compatibility/postgresql/converted/core/create_type.sql` | Composite types |
| `tests/compatibility/postgresql/converted/core/rowtypes.sql` | Row types |
| `tests/compatibility/postgresql/converted/core/rangetypes.sql` | Range types |
| `tests/compatibility/postgresql/converted/core/tstypes.sql` | Text search types |
| `tests/compatibility/mysql/converted/main/type_enum.sql` | ENUM types |
| `tests/compatibility/mysql/converted/main/type_set.sql` | SET types |
| `tests/compatibility/mysql/converted/gis/all_geometry_types_instantiable.sql` | Spatial types |

## Related Specifications

- [scalar_types.md](./scalar_types.md) - Scalar type foundations
- [type_coercion_rules.md](./type_coercion_rules.md) - Type casting rules
- Index specifications (HNSW for vectors, GIN for JSONB, R-tree for spatial)

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| WKB | Well-Known Binary - OGC standard geometry format |
| EWKB | Extended WKB - PostGIS extension with SRID |
| CBOR | Concise Binary Object Representation |
| GIN | Generalized Inverted Index for JSONB/text search |
| HNSW | Hierarchical Navigable Small World - vector index |
| SRID | Spatial Reference ID - coordinate system identifier |

### Configuration Keys

| Key | Default | Description |
|-----|---------|-------------|
| `types.json.validation` | strict | JSON validation mode (strict/lenient) |
| `types.xml.validation` | strict | XML validation mode |
| `vector.index.default_metric` | cosine | Default similarity metric for vectors |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial complex types specification | Dalton Calford |
