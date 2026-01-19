# Parser Audit Documentation

**Purpose:** Comprehensive audit of all four ScratchBird parsers to ensure dialect purity and prevent cross-contamination.

**Date:** 2026-01-07
**Status:** In Progress

---

## Audit Objectives

1. **Dialect Purity**: Ensure each parser ONLY implements its intended SQL dialect
2. **No Bleeding**: Detect any features from one dialect appearing in another
3. **Implementation Truth**: Document what is ACTUALLY implemented, not what comments claim
4. **Context Sensitivity**: Verify V2 parser's context-sensitive keyword handling
5. **FirebirdSQL Style**: Confirm V2 parser follows FirebirdSQL formatting standards

---

## Directory Structure

```
docs/audit/parsers/
├── README.md                    # This file
├── V2/                          # V2 (native ScratchBird) parser audit
│   ├── 01_DDL.md               # DDL statement support
│   ├── 02_DML.md               # DML statement support
│   ├── 03_PSQL.md              # Procedural language support
│   ├── 04_TRANSACTION.md       # Transaction control
│   ├── 05_SECURITY.md          # Security statements
│   ├── 06_ADMIN.md             # Admin commands
│   ├── 07_SHOW_SET.md          # SHOW/SET commands
│   ├── 08_FUNCTIONS.md         # All functions
│   ├── 09_OPERATORS.md         # All operators
│   ├── 10_DATATYPES.md         # Data types
│   ├── 11_CATALOG.md           # Catalog structure (ASCII tree)
│   ├── 12_RESERVED_WORDS.md    # Reserved words and context sensitivity
│   └── SUMMARY.md              # Executive summary with concerns
├── FirebirdSQL/                # FirebirdSQL emulated parser audit
│   ├── 01_DDL.md
│   ├── 02_DML.md
│   ├── 03_PSQL.md
│   ├── 04_TRANSACTION.md
│   ├── 05_SECURITY.md
│   ├── 06_ADMIN.md
│   ├── 07_CONTEXT_VARS.md      # Firebird context variables
│   ├── 08_FUNCTIONS.md
│   ├── 09_OPERATORS.md
│   ├── 10_DATATYPES.md
│   ├── 11_CATALOG.md           # RDB$, MON$, SEC$ tables
│   ├── 12_PACKAGES.md          # Firebird packages
│   └── SUMMARY.md              # Dialect bleeding concerns
├── PostgreSQL/                 # PostgreSQL emulated parser audit
│   ├── 01_DDL.md
│   ├── 02_DML.md
│   ├── 03_PLPGSQL.md          # PL/pgSQL language
│   ├── 04_TRANSACTION.md
│   ├── 05_SECURITY.md
│   ├── 06_ADMIN.md
│   ├── 07_SHOW_SET.md
│   ├── 08_FUNCTIONS.md
│   ├── 09_OPERATORS.md
│   ├── 10_DATATYPES.md
│   ├── 11_CATALOG.md           # pg_catalog, information_schema
│   ├── 12_EXTENSIONS.md        # PostgreSQL extensions
│   └── SUMMARY.md              # Dialect bleeding concerns
├── MySQL/                      # MySQL emulated parser audit
│   ├── 01_DDL.md
│   ├── 02_DML.md
│   ├── 03_PROCEDURES.md        # MySQL stored procedures
│   ├── 04_TRANSACTION.md
│   ├── 05_SECURITY.md
│   ├── 06_ADMIN.md
│   ├── 07_SHOW_SET.md
│   ├── 08_FUNCTIONS.md
│   ├── 09_OPERATORS.md
│   ├── 10_DATATYPES.md
│   ├── 11_CATALOG.md           # information_schema, mysql schema
│   └── SUMMARY.md              # Dialect bleeding concerns
└── COMPARISON_MATRIX.md        # Cross-parser comparison

```

---

## Audit Methodology

### 1. Source Code Analysis

- **Primary Source**: Actual parser implementation code (`.cpp`, `.h` files)
- **DO NOT TRUST**: Comments, documentation, or specifications
- **Focus**: What the code ACTUALLY does, not what it claims to do

### 2. Dialect Verification

For each parser, verify:
- ✅ **Should Have**: All features of its intended dialect
- ❌ **Should NOT Have**: Any features from other dialects
- ⚠️ **Gray Area**: Common SQL features (document explicitly)

### 3. Documentation Format

Each document should include:
- **Statement/Feature Name**
- **Syntax Support** (exact syntax from implementation)
- **Source Location** (file:line)
- **Implementation Details** (brief code excerpt if relevant)
- **Concerns** (any dialect bleeding or anomalies)

### 4. Evidence Required

For any concerns raised:
- Specific file and line number
- Code excerpt showing the issue
- Explanation of why it's a concern
- Severity: CRITICAL / HIGH / MEDIUM / LOW

---

## Parser Descriptions

### V2 Parser (Native ScratchBird)

**Location**: `src/parser/parser_v2.cpp`, `include/scratchbird/parser/parser_v2.h`

**Expected Characteristics**:
- **Base Style**: FirebirdSQL formatting and conventions
- **Extended Features**: Advanced features beyond any single dialect
- **Context Sensitivity**: Should reduce reserved word collisions
- **SBLR Target**: Generates ScratchBird Bytecode Language Runtime code

**Audit Focus**:
- Is it truly context-sensitive?
- Does it follow FirebirdSQL style?
- Are extensions well-documented?
- No accidental MySQL/PostgreSQL-only syntax?

### FirebirdSQL Parser (Emulated)

**Location**: TBD (find during audit)

**Expected Characteristics**:
- **Pure Firebird**: ONLY Firebird SQL dialect
- **RDB$ Catalog**: Firebird system table structure
- **PSQL**: Firebird procedural language
- **Packages**: Firebird package support
- **Context Variables**: Firebird-specific context variables

**Audit Focus**:
- Zero PostgreSQL features
- Zero MySQL features
- Zero V2-specific extensions
- Pure Firebird compatibility

### PostgreSQL Parser (Emulated)

**Location**: `src/parser/postgresql_parser.cpp` or similar

**Expected Characteristics**:
- **Pure PostgreSQL**: ONLY PostgreSQL SQL dialect
- **pg_catalog**: PostgreSQL system catalog
- **PL/pgSQL**: PostgreSQL procedural language
- **PostgreSQL Operators**: `||`, `@>`, `->`, etc.
- **PostgreSQL Types**: SERIAL, JSONB, arrays, etc.

**Audit Focus**:
- Zero Firebird features
- Zero MySQL features
- Zero V2-specific extensions
- Pure PostgreSQL compatibility

### MySQL Parser (Emulated)

**Location**: TBD (find during audit)

**Expected Characteristics**:
- **Pure MySQL**: ONLY MySQL SQL dialect
- **MySQL Catalog**: information_schema, mysql schema
- **MySQL Procedures**: MySQL stored procedure syntax
- **MySQL Operators**: MySQL-specific operators
- **MySQL Types**: TINYINT, ENUM, SET, AUTO_INCREMENT

**Audit Focus**:
- Zero Firebird features
- Zero PostgreSQL features
- Zero V2-specific extensions
- Pure MySQL compatibility

---

## Catalog Structure Documentation

Each parser's catalog documentation should include:

### ASCII Tree Format

```
schema_name/
├── table_name
│   ├── column_name (datatype, constraints)
│   ├── column_name (datatype, constraints)
│   └── indexes
│       ├── index_name (columns, type)
│       └── index_name (columns, type)
├── view_name
│   └── definition summary
└── sequence_name
    └── properties
```

### Full Details

For each catalog object:
- **Name**
- **Type** (table, view, sequence, etc.)
- **Columns** (name, type, nullable, default)
- **Constraints** (primary key, foreign key, unique, check)
- **Indexes**
- **Permissions/Ownership**
- **Dependencies**

---

## Known Concerns (To Be Filled)

This section will be populated as audits complete.

### V2 Parser

- TBD

### FirebirdSQL Parser

- TBD

### PostgreSQL Parser

- TBD

### MySQL Parser

- TBD

---

## Comparison Matrix

See `COMPARISON_MATRIX.md` for cross-parser feature comparison to identify dialect bleeding.

---

## Review Process

1. **Automated Audit**: Explore agents analyze implementation
2. **Documentation**: Generate detailed documentation per parser
3. **Human Review**: Developer reviews documentation for anomalies
4. **Remediation**: Fix any dialect bleeding issues found
5. **Verification**: Re-audit after fixes
6. **Sign-off**: Approve parser purity

---

**Last Updated**: 2026-01-07
**Next Review**: After any parser modifications
