# ScratchBird Version Management

## Current Release Status

### ScratchBird v0.5.0 (Released)
- **Version String**: `SB-T0.5.0.1`
- **Release Date**: July 16, 2025
- **Status**: ✅ **RELEASED** - Production ready
- **Codename**: "Phoenix Rising"

## Version Numbering Scheme

### Format: `MAJOR.MINOR.PATCH.BUILD`
- **MAJOR**: Significant architectural changes or breaking compatibility
- **MINOR**: New features, enhancements, substantial improvements
- **PATCH**: Bug fixes, security updates, minor improvements  
- **BUILD**: Internal build number for tracking

### Version String Format: `SB-T{MAJOR}.{MINOR}.{PATCH}.{BUILD}`
- **SB**: ScratchBird identifier
- **T**: Target/Release identifier
- **Example**: `SB-T0.5.0.1`, `SB-T0.6.0.1`

## v0.6.0 Development Planning

### Target Features for v0.6.0
Based on REMAINING WORK identified in v0.5.0:

#### FEATURE #1: COMMENT ON SCHEMA Support (High Priority)
- **Description**: Complete implementation of schema commenting functionality
- **Components**: Parser extension, metadata storage, DDL support
- **Target**: First milestone for v0.6.0

#### FEATURE #2: Range Types Completion (Medium Priority)  
- **Description**: PostgreSQL-compatible range data types
- **Components**: Parser, storage, functions, operators
- **Target**: Second milestone for v0.6.0

#### FEATURE #3: Enhanced Schema Backup/Restore (Medium Priority)
- **Description**: Selective schema backup with hierarchy support
- **Components**: Enhanced sb_gbak, metadata filtering
- **Target**: Third milestone for v0.6.0

### Additional v0.6.0 Enhancements
- **2PC Monitoring Tools**: Advanced transaction monitoring and management
- **Configuration File Loading**: Complete external data source configuration
- **Performance Optimizations**: Large transaction 2PC recovery optimization
- **Documentation Expansion**: Comprehensive advanced feature documentation

## Version Control Strategy

### Branch Structure
```
main                    # Stable release branch (v0.5.0)
├── develop            # Integration branch for v0.6.0 features
├── feature/comment-on-schema    # FEATURE #1 development
├── feature/range-types          # FEATURE #2 development  
├── feature/enhanced-backup      # FEATURE #3 development
└── hotfix/*           # Emergency fixes for v0.5.x
```

### Release Branches
- `release/v0.5.x` - Maintenance releases for v0.5 series
- `release/v0.6.0` - Preparation branch for v0.6.0 release

## Build Version Management

### v0.6.0 Development Versioning
```
SB-T0.6.0.1-dev     # First development build
SB-T0.6.0.2-dev     # Incremental development builds
...
SB-T0.6.0.50-alpha  # Alpha releases
SB-T0.6.0.75-beta   # Beta releases  
SB-T0.6.0.100-rc1   # Release candidates
SB-T0.6.0.1         # Final release
```

### Build Number Allocation
- **1-49**: Development builds
- **50-74**: Alpha releases
- **75-99**: Beta releases  
- **100+**: Release candidates and final

## File Version Updates Required

### Source Code Headers
```cpp
// Update in src/include/sb_version.h
#define SB_MAJOR_VERSION    0
#define SB_MINOR_VERSION    6  
#define SB_PATCH_VERSION    0
#define SB_BUILD_VERSION    1
```

### Build Configuration
```makefile
# Update in gen/Make.Version
SB_VERSION_MAJOR=0
SB_VERSION_MINOR=6
SB_VERSION_PATCH=0
SB_VERSION_BUILD=1
```

### Documentation Updates
- `CHANGELOG.md` - Add v0.6.0 section
- `README.md` - Update version references
- Release documentation templates

## Release Schedule Projection

### v0.6.0 Development Timeline
```
August 2025     # FEATURE #1: COMMENT ON SCHEMA (4 weeks)
September 2025  # FEATURE #2: Range Types (6 weeks)  
October 2025    # FEATURE #3: Enhanced Backup (4 weeks)
November 2025   # Integration, testing, documentation (4 weeks)
December 2025   # Alpha/Beta releases (6 weeks)
January 2026    # Release candidate and final release (2 weeks)
```

### Milestone Schedule
- **M1** (Aug 31, 2025): COMMENT ON SCHEMA complete
- **M2** (Oct 15, 2025): Range Types complete
- **M3** (Nov 15, 2025): Enhanced Backup complete
- **M4** (Dec 15, 2025): Feature freeze, alpha release
- **M5** (Jan 15, 2026): v0.6.0 final release

## Compatibility Matrix

### v0.5.x Maintenance
- **v0.5.1**: Bug fixes, security updates (if needed)
- **v0.5.2**: Minor improvements, compatibility updates
- **EOL**: June 2026 (6 months after v0.6.0 release)

### v0.6.x Support
- **v0.6.0**: Major release with new features
- **v0.6.1**: First patch release (bug fixes)
- **LTS**: Long-term support until v0.8.0 (estimated 2027)

## Development Environment Setup

### Version Switching
```bash
# Switch to v0.6.0 development
git checkout develop
git pull origin develop

# Update version in build system
echo "0.6.0" > VERSION
make clean
make TARGET=Release
```

### Build Verification
```bash
# Verify version strings
./gen/Release/scratchbird/bin/sb_isql -z
# Should show: "isql version SB-T0.6.0.1-dev ScratchBird 0.6"
```

## Quality Gates for v0.6.0

### Code Quality Requirements
- [ ] All new features have comprehensive test coverage
- [ ] Performance regression testing against v0.5.0 baseline
- [ ] Memory leak detection and resolution
- [ ] Static analysis with zero critical issues

### Documentation Requirements  
- [ ] API documentation for all new features
- [ ] Migration guide from v0.5.x to v0.6.0
- [ ] Performance tuning guide updates
- [ ] Security best practices documentation

### Testing Requirements
- [ ] Automated test suite with 95%+ pass rate
- [ ] Cross-platform compatibility verification
- [ ] Load testing with enterprise workloads
- [ ] Security vulnerability assessment

---

**Next Actions for v0.6.0 Development:**

1. **Create Development Branch**: Set up `develop` branch for v0.6.0
2. **Update Version Numbers**: Increment to v0.6.0-dev in build system
3. **Feature Planning**: Detailed planning for COMMENT ON SCHEMA implementation
4. **Environment Setup**: Configure development environment for v0.6.0 work

**Note**: v0.5.0 is now production-ready and available for release. v0.6.0 development can begin immediately with the established branch strategy.