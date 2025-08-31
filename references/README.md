# ScratchBird Reference Materials

This directory contains all technical reference materials, specifications, and documentation needed for implementing ScratchBird.

## Directory Structure

### `/technical_specifications/`
Core technical specifications for ScratchBird implementation:
- `PAGE_LAYOUTS_AND_STRUCTURES.md` - Page headers, layouts for all page types
- `REPLICATION_AND_SHADOW_PROTOCOLS.md` - Shadow database and replication protocols
- `WIRE_PROTOCOL_SPECIFICATIONS.md` - Wire protocol requirements for Y-Valve

### `/db_interface/`
Database client interface specifications (for reference):
- `firebird_spec.md` - Firebird client API usage
- `postgresql_spec.md` - PostgreSQL client API usage
- `mysql_mariadb_spec.md` - MySQL/MariaDB client API usage
- `mssql_spec.md` - MSSQL client API usage
- `odbc_generic_spec.md` - ODBC interface usage
- `jdbc_jni_spec.md` - JDBC/JNI interface usage
- `unified_interface_spec.md` - Unified C++ interface design

### `/wire_protocols/` (To be added)
Detailed wire protocol specifications:
- PostgreSQL Frontend/Backend Protocol v3
- MySQL Client/Server Protocol
- Firebird Remote Protocol
- TDS (Tabular Data Stream) Protocol

### `/sql_dialects/` (To be added)
SQL dialect specifications for each supported database:
- PostgreSQL SQL extensions
- MySQL/MariaDB SQL variations
- Firebird SQL/PSQL
- T-SQL (MSSQL)
- ScratchBird core SQL

### `/data_types/` (To be added)
Data type mappings and specifications:
- Native type specifications for each database
- Type conversion matrices
- Binary representations
- Collation and character sets

## Reference Material Categories

### 1. Technical Specifications (Internal)
Located in `/technical_specifications/`, these define how ScratchBird implements its features:
- Page structures and layouts
- Storage formats
- Internal protocols
- Engine architecture

### 2. External Protocol Specifications
Standards and protocols ScratchBird must implement for compatibility:
- Wire protocols (PostgreSQL, MySQL, etc.)
- Authentication methods
- Encryption standards
- Network protocols

### 3. SQL Standards and Dialects
SQL language specifications and dialect differences:
- ANSI SQL standards
- Database-specific extensions
- Function libraries
- Procedural languages

### 4. Implementation References
Source materials for implementation guidance:
- Algorithm descriptions
- Best practices
- Performance optimization techniques
- Security guidelines

## Usage Guidelines

### For Developers
1. **Reference, don't modify** - These are source materials
2. **Link from ProjectPlan** - Planning docs should reference these specs
3. **Keep versions** - When updating specs, consider versioning
4. **Document sources** - Include links to original sources when applicable

### For AI Implementation
1. **Single source of truth** - All technical details here
2. **No duplication** - Don't copy specs into implementation
3. **Reference by path** - Use full paths when referencing
4. **Check for updates** - Specs may be updated as project evolves

## Adding New References

When adding new reference materials:

1. **Choose correct subdirectory** based on type
2. **Use clear naming** - Be specific and descriptive
3. **Include metadata** - Version, source, date at top of document
4. **Update this README** - Add to appropriate section
5. **Link from ProjectPlan** - Update relevant planning documents

## Key Reference Documents

### Essential for Implementation
1. `PAGE_LAYOUTS_AND_STRUCTURES.md` - Core storage format
2. `WIRE_PROTOCOL_SPECIFICATIONS.md` - Protocol requirements
3. `REPLICATION_AND_SHADOW_PROTOCOLS.md` - HA/DR implementation

### Architecture Decisions
Located in `/workspace/docs/architecture/`:
- `ADR-001-MGA-Over-Traditional-MVCC.md`
- `ADR-002-UUID-Based-Schema.md`
- `Layered_Architecture_Complete.md`
- `Distributed_Infrastructure_Design.md`

### Feature Specifications
Located in `/workspace/docs/specifications/`:
- `Advanced_Trigger_System.md`
- `Context_Aware_Parser.md`
- `Event_Notification_System.md`
- `Metadata_Version_Control_System.md`

## External Resources Needed

### Priority 1 - Wire Protocols
- [ ] PostgreSQL Frontend/Backend Protocol (official docs)
- [ ] MySQL Client/Server Protocol (MySQL internals)
- [ ] Firebird Remote Protocol (source code)
- [ ] TDS Specification (FreeTDS docs)

### Priority 2 - SQL Specifications
- [ ] SQL:2016 Standard
- [ ] PostgreSQL SQL Reference
- [ ] MySQL SQL Reference
- [ ] Firebird SQL Reference

### Priority 3 - Implementation Details
- [ ] B-tree algorithms
- [ ] MGA/MVCC papers
- [ ] WAL implementation guides
- [ ] Network programming best practices

## Notes

- Technical specifications are **reference materials**, not planning documents
- Implementation details go in source code, not here
- These specs inform but don't dictate implementation
- Keep specifications focused and technical