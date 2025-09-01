# Technical References for Implementation

This document links to the technical specifications in the `/references/` directory that should be consulted during implementation.

## Core Technical Specifications

### Storage and Page Management
**Reference**: `/workspace/references/archive/technical_specifications/PAGE_LAYOUTS_AND_STRUCTURES.md`

Key specifications:
- 96-byte page header format
- 32 different page types (DATA, INDEX, BLOB, etc.)
- Page flags for replication and storage tiering
- Index-specific page layouts (B-tree, Hash, Bitmap, GIN, R-tree, LSM, etc.)
- Filespace and tablespace structures

### Replication and High Availability
**Reference**: `/workspace/references/technical_specifications/REPLICATION_AND_SHADOW_PROTOCOLS.md`

Key specifications:
- Shadow database (physical replication)
- Dual-channel replication (pages + WAL via Kafka)
- Logical replication protocols
- Failover and promotion procedures

### Wire Protocols and Compatibility
**Reference**: `/workspace/references/technical_specifications/WIRE_PROTOCOL_SPECIFICATIONS.md`

Key specifications:
- PostgreSQL wire protocol v3 requirements
- MySQL/MariaDB protocol requirements
- Firebird protocol requirements
- TDS (MSSQL) protocol requirements
- Y-Valve routing design

## Implementation Phases and Technical Specs

### Phase 1.01 - Foundation
Consult:
- Page header format (PAGE_LAYOUTS_AND_STRUCTURES.md)
- Root page and header page layouts
- Database file structure

### Phase 1.02 - Buffer Pool
Consult:
- Page flags for buffer management
- HOT/COLD flags for tiering
- PINNED flag for buffer pool

### Phase 1.03 - Heap Storage
Consult:
- DATA_PAGE layout
- Tuple header format (24 bytes minimum)
- Line pointer array structure

### Phase 1.05 - B-Tree Index
Consult:
- BTREE_LEAF and BTREE_INTERNAL page layouts
- B-tree special area (16 bytes)
- Index tuple format

### Phase 1.09 - MGA Implementation
Consult:
- Transaction ID fields in tuple headers
- Shadow LSN in page headers
- Version chain management

### Phase 1.16 - WAL Secondary
Consult:
- WAL_BUFFER page layout
- Dual-channel replication design
- Kafka integration for WAL streaming

### Phase 2.x - Advanced Indexes
Consult page layouts for:
- Hash index (HASH_BUCKET, HASH_OVERFLOW)
- Bitmap index (BITMAP_PAGE, BITMAP_META)
- GIN index (GIN_ENTRY, GIN_DATA)
- R-tree (RTREE_NODE, RTREE_META)
- LSM tree (LSM_L0, LSM_LN)
- Columnstore (COLUMN_DATA, COLUMN_DICT)

### Phase 3.x - Protocol Implementation
Consult:
- Wire protocol specifications for each database
- Y-Valve routing requirements
- Protocol detection and handling

### Phase 4.x - Replication
Consult:
- Shadow database specifications
- Replication status page layout
- Shadow map page layout
- Kafka checkpoint page layout

## Cross-Reference Guide

### When implementing a feature, check:

1. **Page Structure**
   - Is there a specific page type defined?
   - What are the header requirements?
   - Are there special flags needed?

2. **Replication Impact**
   - Does this need shadow LSN tracking?
   - Should it use replication flags?
   - How does it work with WAL streaming?

3. **Protocol Compatibility**
   - How do other databases handle this?
   - What wire protocol messages are needed?
   - How does Y-Valve route this operation?

4. **Storage Location**
   - Can this use multiple filespaces?
   - How does tablespace assignment work?
   - What are the tiering implications?

## Technical Specification Locations

All technical specifications are maintained in:
```
/workspace/references/
├── technical_specifications/
│   ├── PAGE_LAYOUTS_AND_STRUCTURES.md
│   ├── REPLICATION_AND_SHADOW_PROTOCOLS.md
│   └── WIRE_PROTOCOL_SPECIFICATIONS.md
├── db_interface/
│   └── [Client interface specifications]
└── README.md
```

## Important Notes

1. **Reference, don't duplicate** - Always link to specs, don't copy content
2. **Check for updates** - Specifications may be refined as implementation progresses
3. **Validate assumptions** - If something seems unclear, check the specification
4. **Report issues** - If specs are inconsistent or incomplete, document the issue

## Specification Versioning

Current specification versions:
- Page Layouts: v1.0 (includes replication support)
- Replication Protocols: v1.0 (dual-channel design)
- Wire Protocols: v1.0 (requirements defined, details pending)