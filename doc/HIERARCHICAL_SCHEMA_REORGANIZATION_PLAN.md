# ScratchBird Hierarchical Schema Reorganization Plan

**Document Version**: 1.0  
**Date**: July 14, 2025  
**Status**: APPROVED FOR IMPLEMENTATION

## Overview

This document outlines a comprehensive plan to reorganize ScratchBird's default schema structure into a hierarchical, logically organized system that improves upon the current flat schema model while maintaining SQL standards compliance and administrative clarity.

## Proposed Schema Hierarchy

### Final Target Structure
```
ROOT/
├── SYSTEM/
│   ├── INFORMATION_SCHEMA/     # SQL standard compliant views (ANSI SQL)
│   ├── HIERARCHY/              # Schema management tools and utilities
│   ├── PLG$LEGACY_SEC/         # Legacy security plugin objects
│   └── PLG$SRP/                # SRP authentication plugin objects
├── USERS/
│   ├── PUBLIC/                 # Default user schema (existing)
│   └── [username]/             # Optional user-specific schemas (created on demand)
├── LINKS/                      # Default location for database links
└── DATABASE/
    ├── MONITORING/             # Human-readable views over MON$ tables
    ├── PROGRAMMING/            # Common procedures, functions, packages
    └── TRIGGERS/               # Database-level triggers and utilities
```

### Schema Purposes and Contents

**SYSTEM Hierarchy**
- `SYSTEM.INFORMATION_SCHEMA`: SQL standard compliant metadata views (TABLES, COLUMNS, ROUTINES, etc.)
- `SYSTEM.HIERARCHY`: Administrative functions for schema management, validation, and navigation
- `SYSTEM.PLG$LEGACY_SEC`: Relocated legacy security plugin objects
- `SYSTEM.PLG$SRP`: Relocated SRP authentication plugin objects

**USERS Hierarchy**
- `USERS.PUBLIC`: Current PUBLIC schema (maintains backward compatibility)
- `USERS.[username]`: Optional user-specific schemas created when users are added to database

**LINKS Schema**
- Default mount point for database links
- Schema-aware database link management utilities
- Link resolution and validation tools

**DATABASE Hierarchy**
- `DATABASE.MONITORING`: Enhanced, human-readable views over MON$ system tables
- `DATABASE.PROGRAMMING`: Common procedures, functions, packages used database-wide
- `DATABASE.TRIGGERS`: Database-level triggers, event handlers, and utility triggers

## Implementation Strategy

### Phase 1: Foundation (Immediate - Low Risk)
**Objective**: Add new organizational schemas without breaking existing functionality

```sql
-- Add new database organizational schemas
CREATE SCHEMA DATABASE;
CREATE SCHEMA DATABASE.MONITORING;
CREATE SCHEMA DATABASE.PROGRAMMING;
CREATE SCHEMA DATABASE.TRIGGERS;

-- Add database links schema
CREATE SCHEMA LINKS;
```

**Implementation Tasks**:
1. Extend database initialization to create new schemas
2. Implement enhanced MON$ views in DATABASE.MONITORING
3. Create common utility procedures in DATABASE.PROGRAMMING
4. Add database link management in LINKS schema

### Phase 2: System Enhancement (Near-term - Medium Risk)
**Objective**: Implement SQL standards compliance and schema management tools

```sql
-- Add SQL standards compliance
CREATE SCHEMA SYSTEM.INFORMATION_SCHEMA;
CREATE SCHEMA SYSTEM.HIERARCHY;
```

**Implementation Tasks**:
1. Implement INFORMATION_SCHEMA views (TABLES, COLUMNS, ROUTINES, SCHEMATA, etc.)
2. Create hierarchical schema management utilities
3. Add schema validation and navigation tools
4. Implement schema discovery and documentation functions

### Phase 3: User Schema Enhancement (Medium-term - Medium Risk)
**Objective**: Reorganize user schema structure with optional user-specific schemas

```sql
-- Add user organization
CREATE SCHEMA USERS;
-- PUBLIC schema becomes USERS.PUBLIC (with alias for backward compatibility)
-- Optional user schemas created on demand
```

**Implementation Tasks**:
1. Create USERS parent schema
2. Implement user schema creation on user addition
3. Add schema alias system for backward compatibility
4. Update user management tools (sb_gsec) to support schema creation

### Phase 4: Plugin Reorganization (Long-term - High Risk)
**Objective**: Move plugin objects into SYSTEM hierarchy

```sql
-- Move plugin objects to new hierarchy
-- PLG$LEGACY_SEC becomes SYSTEM.PLG$LEGACY_SEC
-- PLG$SRP becomes SYSTEM.PLG$SRP
```

**Implementation Tasks**:
1. Implement plugin object migration utilities
2. Update plugin system to use new schema locations
3. Create backward compatibility layer
4. Update authentication systems

## Technical Requirements

### Database Initialization Changes
**File**: `src/jrd/ini.epp`
- Extend `INI_format()` to create new schema hierarchy
- Add schema creation functions for each component
- Implement proper dependency ordering

### Schema Constants Updates
**File**: `src/jrd/constants.h`
```cpp
// New schema constants
constexpr const char* DATABASE_SCHEMA = "DATABASE";
constexpr const char* DATABASE_MONITORING_SCHEMA = "DATABASE.MONITORING";
constexpr const char* DATABASE_PROGRAMMING_SCHEMA = "DATABASE.PROGRAMMING";
constexpr const char* DATABASE_TRIGGERS_SCHEMA = "DATABASE.TRIGGERS";
constexpr const char* USERS_SCHEMA = "USERS";
constexpr const char* USERS_PUBLIC_SCHEMA = "USERS.PUBLIC";
constexpr const char* LINKS_SCHEMA = "LINKS";
constexpr const char* SYSTEM_INFORMATION_SCHEMA = "SYSTEM.INFORMATION_SCHEMA";
constexpr const char* SYSTEM_HIERARCHY_SCHEMA = "SYSTEM.HIERARCHY";
```

### Schema Resolution Updates
**File**: `src/jrd/Attachment.cpp`
- Update hierarchical validation to support new structure
- Implement schema alias resolution for backward compatibility
- Add user schema creation triggers

## Backward Compatibility Strategy

### Schema Aliases
- Maintain `PUBLIC` as alias to `USERS.PUBLIC`
- Create virtual views for relocated objects
- Implement transparent redirection for common operations

### Tool Updates
- Update ISQL to support new schema navigation
- Modify GBAK to handle hierarchical backup/restore
- Enhance GSEC for user schema management

### Migration Path
- Automatic migration during database upgrade
- Optional migration tools for existing databases
- Rollback capabilities for problematic migrations

## Implementation Priorities

### High Priority (Immediate Benefits)
1. **DATABASE.MONITORING**: Enhanced monitoring and diagnostics
2. **DATABASE.PROGRAMMING**: Common utility procedures
3. **LINKS**: Database link management
4. **SYSTEM.INFORMATION_SCHEMA**: SQL standards compliance

### Medium Priority (Administrative Benefits)
1. **SYSTEM.HIERARCHY**: Schema management tools
2. **USERS** reorganization: Better user organization
3. **Plugin reorganization**: Cleaner system structure

### Lower Priority (Long-term Goals)
1. **Complete migration tools**: Full backward compatibility
2. **Performance optimization**: Cache optimization for new structure
3. **Enhanced security**: Schema-based security model

## Risk Assessment

### Low Risk Components
- DATABASE.* schemas (additive only)
- LINKS schema (new functionality)
- SYSTEM.INFORMATION_SCHEMA (standards compliance)

### Medium Risk Components  
- USERS reorganization (affects user workflows)
- Plugin reorganization (compatibility concerns)

### High Risk Components
- Fundamental SYSTEM schema changes
- Complete migration of existing databases
- Security model modifications

## Success Metrics

### Functional Metrics
- All existing functionality preserved
- New organizational structure fully functional
- SQL standards compliance achieved
- Enhanced administrative capabilities

### Performance Metrics
- Schema resolution performance maintained or improved
- Database initialization time acceptable
- Memory usage for schema cache optimized

### Compatibility Metrics
- 100% backward compatibility for existing applications
- Seamless migration for existing databases
- Tool compatibility maintained

## Next Steps

1. **Approve this plan** and begin Phase 1 implementation
2. **Create detailed implementation tickets** for each component
3. **Set up testing framework** for hierarchical schema changes
4. **Begin with DATABASE.MONITORING** implementation as proof of concept
5. **Establish migration testing** with sample databases

## Notes

- **User Schema Creation**: USERS.[username] schemas are optional and created only when requested during user addition
- **Performance Considerations**: Deep hierarchy designed to minimize impact on existing schema resolution performance
- **SQL Standards**: INFORMATION_SCHEMA implementation will provide full ANSI SQL compliance
- **Extensibility**: Framework designed to support future organizational enhancements

This plan provides a structured approach to modernizing ScratchBird's schema organization while maintaining stability and backward compatibility. The phased implementation ensures manageable risk and allows for course correction as needed.