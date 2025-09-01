# Source Code Reference Strategy

## Recommended Approach: Selective Extraction

### DO NOT: Copy Full Source Code
- **Size**: PostgreSQL ~200MB, MySQL ~2GB source
- **Legal**: Some have GPL licenses (MySQL)
- **Maintenance**: Becomes outdated quickly
- **Noise**: 99% irrelevant to your needs

### DO: Extract Key References

## 1. Parser Grammars Only

```bash
# Create grammar reference directory
references/
├── grammars/
│   ├── postgresql_gram.y     # From src/backend/parser/gram.y
│   ├── mysql_sql_yacc.yy     # From sql/sql_yacc.yy
│   ├── firebird_parse.y      # From src/dsql/parse.y
│   └── mariadb_sql_yacc.yy   # From sql/sql_yacc.yy
```

## 2. Protocol Definitions

```bash
references/
├── protocols/
│   ├── postgresql/
│   │   ├── protocol.h        # From src/include/libpq/pqcomm.h
│   │   └── fe-protocol3.c    # Key functions only
│   ├── mysql/
│   │   ├── mysql_com.h       # Protocol constants
│   │   └── protocol_classic.cc # Key message formats
│   └── firebird/
│       ├── protocol.h        # From src/remote/protocol.h
│       └── wire.cpp          # Key wire format functions
```

## 3. Data Structure Definitions

```bash
references/
├── structures/
│   ├── postgresql/
│   │   ├── htup.h           # Heap tuple format
│   │   ├── page.h           # Page layout
│   │   └── snapshot.h       # MVCC structures
│   ├── firebird/
│   │   ├── ods.h            # On-disk structure
│   │   ├── RecordNumber.h   # MGA structures
│   │   └── tra.h            # Transaction structures
│   └── mysql/
│       ├── page0page.h      # InnoDB page format
│       └── row0row.h        # Row format
```

## 4. Key Algorithms

```bash
references/
├── algorithms/
│   ├── mga_visibility.cpp    # Firebird's MGA visibility
│   ├── mvcc_snapshot.c       # PostgreSQL's snapshot
│   ├── btree_insert.c        # B-tree algorithms
│   └── join_hash.cpp         # Hash join implementation
```

## Extraction Script

```bash
#!/bin/bash
# extract_references.sh

# Clone repositories to temp location
TEMP_DIR=/tmp/db_sources
mkdir -p $TEMP_DIR

# Clone sources
git clone --depth 1 https://github.com/postgres/postgres $TEMP_DIR/postgres
git clone --depth 1 https://github.com/mysql/mysql-server $TEMP_DIR/mysql
git clone --depth 1 https://github.com/FirebirdSQL/firebird $TEMP_DIR/firebird
git clone --depth 1 https://github.com/MariaDB/server $TEMP_DIR/mariadb

# Extract grammars
cp $TEMP_DIR/postgres/src/backend/parser/gram.y references/grammars/postgresql_gram.y
cp $TEMP_DIR/mysql/sql/sql_yacc.yy references/grammars/mysql_sql_yacc.yy
cp $TEMP_DIR/firebird/src/dsql/parse.y references/grammars/firebird_parse.y

# Extract key headers
cp $TEMP_DIR/postgres/src/include/libpq/pqcomm.h references/protocols/postgresql/
cp $TEMP_DIR/mysql/include/mysql_com.h references/protocols/mysql/
cp $TEMP_DIR/firebird/src/remote/protocol.h references/protocols/firebird/

# Extract MGA implementation
cp $TEMP_DIR/firebird/src/jrd/tra.cpp references/algorithms/firebird_mga.cpp
cp $TEMP_DIR/firebird/src/jrd/vio.cpp references/algorithms/firebird_vio.cpp

# Clean up
rm -rf $TEMP_DIR
```

## Version Control Strategy

```yaml
# references/sources.yaml
sources:
  postgresql:
    version: "16.1"
    commit: "a1b9b14cc8c2b07e6e57e7e70b9c2b3a4f8c7d9e"
    extracted: "2024-01-15"
    files:
      - path: "src/backend/parser/gram.y"
        local: "grammars/postgresql_gram.y"
        purpose: "SQL grammar reference"
        
  firebird:
    version: "5.0"
    commit: "b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1"
    extracted: "2024-01-15"
    files:
      - path: "src/jrd/tra.cpp"
        local: "algorithms/firebird_mga.cpp"
        purpose: "MGA transaction implementation"
```

## Legal Considerations

```markdown
# references/LICENSE_NOTICE.md

## PostgreSQL
- License: PostgreSQL License (BSD-style)
- Permission: Can use code with attribution
- Attribution: Copyright PostgreSQL Global Development Group

## Firebird
- License: Initial Developer's Public License (IDPL)
- Permission: Can use code with attribution
- Attribution: Copyright Firebird Foundation

## MySQL
- License: GPL v2
- WARNING: Cannot copy code directly
- Action: Reference only, clean-room implementation required

## MariaDB
- License: GPL v2
- WARNING: Cannot copy code directly
- Action: Reference only, clean-room implementation required
```

## What to Extract

### High Priority (Extract These)
1. **Grammar files** (.y, .yy files)
2. **Protocol headers** (message formats)
3. **On-disk structures** (page/row formats)
4. **MGA implementation** (Firebird's tra.cpp, vio.cpp)
5. **System catalog definitions**

### Medium Priority
1. **Optimizer code** (key algorithms only)
2. **Index implementations** (B-tree, GIN, etc.)
3. **Join algorithms**
4. **Parser utilities**

### Low Priority (Reference Online)
1. **Build systems**
2. **Test suites**
3. **Documentation**
4. **Platform-specific code**

## Tools for Analysis

```bash
# Instead of copying, use analysis tools

# Generate call graphs
cscope -b -R $SOURCE_DIR

# Extract structure definitions
ctags -R --c++-kinds=+p --fields=+iaS --extra=+q $SOURCE_DIR

# Find specific patterns
grep -r "MGA\|MultiGeneration" $FIREBIRD_SOURCE

# Generate documentation
doxygen -g Doxyfile && doxygen
```

## Repository References

Instead of copying, maintain links:

```yaml
# references/repositories.yaml
repositories:
  postgresql:
    url: https://github.com/postgres/postgres
    browse: https://github.com/postgres/postgres/tree/master/src
    docs: https://www.postgresql.org/docs/current/
    
  firebird:
    url: https://github.com/FirebirdSQL/firebird
    browse: https://github.com/FirebirdSQL/firebird/tree/master/src
    docs: http://www.firebirdsql.org/file/documentation/
    
  mysql:
    url: https://github.com/mysql/mysql-server
    browse: https://github.com/mysql/mysql-server/tree/8.0/sql
    docs: https://dev.mysql.com/doc/internals/en/
```

## Summary

**Best Practice**: Extract only what you need:
- Grammar files (< 1MB each)
- Protocol definitions (< 100KB each)
- Key algorithms (< 10MB total)
- Total extracted: ~20MB vs ~3GB full source

This approach is:
- Legally safer
- Easier to maintain
- Focused on what you need
- Version controlled
- Searchable and indexed