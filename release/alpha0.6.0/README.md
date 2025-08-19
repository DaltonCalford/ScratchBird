# ScratchBird Alpha 0.6.0 Release

**Release Date**: July 24, 2025  
**Version**: Alpha 0.6.0  
**Build Status**: Development Build  

## What's New in Alpha 0.6.0

### Major Features

1. **Partial Hash Indexes** - O(1) hash-based access with WHERE clause filtering
   - Advanced key generation strategies (5 algorithms)  
   - Compression support (RLE, Delta, Huffman)
   - Performance monitoring and statistics
   - Complete SQL syntax support

2. **Enhanced GIN Indexes** - Advanced text search capabilities
   - Unicode tokenization support
   - Advanced compression algorithms
   - Performance optimizations

3. **Hierarchical Schema Support** - PostgreSQL-style nested schemas
   - Deep schema nesting (up to 8 levels)
   - Complete DDL support
   - Schema-aware database links

4. **Schema-Aware Database Links** - Enhanced remote database access
   - 5 schema resolution modes
   - Context-aware schema mapping
   - High-performance caching

### Platform Support

| Platform | Architecture | Status |
|----------|--------------|--------|
| Linux    | x86_64      | ✅ Primary |
| Linux    | x86         | ✅ Supported |
| Windows  | x86_64      | ✅ Supported |
| macOS    | x86_64      | ✅ Supported |

## Directory Structure

Each platform directory contains:

```
release/alpha0.6.0/[platform]/
├── bin/           # Executables
│   ├── scratchbird        # Database server
│   ├── sb_isql           # Interactive SQL
│   ├── sb_gbak           # Backup/restore
│   ├── sb_gfix           # Database repair
│   ├── sb_gstat          # Statistics
│   └── sb_gsec           # Security manager
├── lib/           # Libraries
│   ├── libsbclient.so    # Client library
│   └── plugins/          # Plugin libraries
├── include/       # Header files
├── doc/           # Documentation
├── examples/      # Sample code and configurations
└── etc/           # Configuration files
    ├── scratchbird.conf
    └── sbintl.conf
```

## Installation

### Linux
```bash
cd release/alpha0.6.0/linux-x64
export SCRATCHBIRD_HOME=$(pwd)
export PATH=$SCRATCHBIRD_HOME/bin:$PATH
export LD_LIBRARY_PATH=$SCRATCHBIRD_HOME/lib:$LD_LIBRARY_PATH
```

### Windows
```cmd
cd release\alpha0.6.0\windows-x64
set SCRATCHBIRD_HOME=%cd%
set PATH=%SCRATCHBIRD_HOME%\bin;%PATH%
```

### macOS
```bash
cd release/alpha0.6.0/darwin-x64
export SCRATCHBIRD_HOME=$(PWD)
export PATH=$SCRATCHBIRD_HOME/bin:$PATH
export DYLD_LIBRARY_PATH=$SCRATCHBIRD_HOME/lib:$DYLD_LIBRARY_PATH
```

## Development Notes

This is an **alpha release** intended for:
- Feature testing and validation
- Performance benchmarking  
- Development feedback
- Integration testing

**Not recommended for production use.**

## Key Implementation Status

- **Partial Hash Indexes**: 95% complete, ready for testing
- **GIN Indexes**: Functional with advanced features
- **Hierarchical Schemas**: Complete implementation
- **Database Links**: Complete with schema awareness
- **Build System**: Compilation fixes applied

## Testing

Comprehensive test suite available in:
- `tests/test_partial_hash_indexes.sh` - Partial hash index testing
- `tests/run_gin_tests.sh` - GIN index testing  
- `tests/test_hierarchical_schemas.sh` - Schema testing
- `tests/test_database_links.sh` - Database link testing

## Documentation

Complete documentation available in `doc/` directory:
- Implementation status documents
- SQL syntax references
- Developer guides
- API documentation

## Support

For development questions and issues:
- Check `STARTING_DEV.md` for project structure
- Review test files for usage examples
- Refer to implementation status documents in `doc/`

---

**Alpha 0.6.0** - ScratchBird Development Team  
*Advanced Database Technology*