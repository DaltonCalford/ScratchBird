#!/bin/bash

# ScratchBird Migration Script
# Migrates Firebird 6.0.0.929 to ScratchBird v0.5 with SQL Dialect 4 enhancements

set -e  # Exit on any error

# Configuration
SOURCE_DIR="/home/dcalford/Documents/claude/Firebird-6.0.0.929-f90eae0-source"
TARGET_DIR="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird"
BACKUP_DIR="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird_backup_$(date +%Y%m%d_%H%M%S)"

echo "🔥 ScratchBird Migration Script v0.5"
echo "Source: $SOURCE_DIR"
echo "Target: $TARGET_DIR"
echo "Backup: $BACKUP_DIR"
echo

# Verify source directory exists
if [[ ! -d "$SOURCE_DIR" ]]; then
    echo "❌ Error: Source directory not found: $SOURCE_DIR"
    exit 1
fi

# Create backup of existing target if it exists
if [[ -d "$TARGET_DIR" ]]; then
    echo "📦 Creating backup of existing ScratchBird directory..."
    cp -r "$TARGET_DIR" "$BACKUP_DIR"
    echo "✅ Backup created: $BACKUP_DIR"
fi

# Create target directory structure
echo "📁 Creating ScratchBird directory structure..."
mkdir -p "$TARGET_DIR"

# Create main directories
mkdir -p "$TARGET_DIR"/{builds/{cmake,posix,install},src,extern,doc,examples,tests}

# Create source subdirectories
mkdir -p "$TARGET_DIR"/src/{jrd,dsql,common,yvalve,remote,auth,burp,isql,alice,utilities/{gsec,gstat,nbackup,fbsvcmgr,fbtracemgr},plugins}

# Create external dependency directories
mkdir -p "$TARGET_DIR"/extern/{boost,decNumber,icu,editline,libtomcrypt,btyacc,cloop}

# Create documentation directories
mkdir -p "$TARGET_DIR"/doc/{sql.extensions}

# Create test directories
mkdir -p "$TARGET_DIR"/tests/{sql_dialect_4,hierarchical_schemas,core}

echo "✅ Directory structure created"

# Function to copy files with progress and exclusions
copy_with_exclusions() {
    local src="$1"
    local dst="$2" 
    local desc="$3"
    
    if [[ ! -d "$src" ]]; then
        echo "⚠️  Warning: Source directory not found: $src"
        return 0
    fi
    
    echo "📂 Copying $desc..."
    
    # Use rsync with exclusions for efficient copying
    rsync -av --progress \
        --exclude='*.o' \
        --exclude='*.a' \
        --exclude='*.so' \
        --exclude='*.exe' \
        --exclude='*.dll' \
        --exclude='*.lib' \
        --exclude='*.obj' \
        --exclude='CMakeCache.txt' \
        --exclude='CMakeFiles/' \
        --exclude='*.log' \
        --exclude='*.bak' \
        --exclude='*.tmp' \
        --exclude='.git*' \
        --exclude='build_test/' \
        --exclude='temp/' \
        --exclude='gen/' \
        "$src"/ "$dst"/
        
    echo "✅ $desc copied"
}

# Phase 1: Copy essential core files
echo
echo "🚀 Phase 1: Copying essential core components..."

# Copy root configuration files
echo "📄 Copying root configuration files..."
cp "$SOURCE_DIR"/CMakeLists.txt "$TARGET_DIR"/ 2>/dev/null || echo "⚠️  CMakeLists.txt not found"
cp "$SOURCE_DIR"/configure "$TARGET_DIR"/ 2>/dev/null || echo "⚠️  configure not found"
cp "$SOURCE_DIR"/configure.ac "$TARGET_DIR"/ 2>/dev/null || echo "⚠️  configure.ac not found"
cp "$SOURCE_DIR"/Makefile "$TARGET_DIR"/ 2>/dev/null || echo "⚠️  Makefile not found"
cp "$SOURCE_DIR"/Makefile.in "$TARGET_DIR"/ 2>/dev/null || echo "⚠️  Makefile.in not found"

# Copy build system
copy_with_exclusions "$SOURCE_DIR/builds/cmake" "$TARGET_DIR/builds/cmake" "CMake build system"
copy_with_exclusions "$SOURCE_DIR/builds/posix" "$TARGET_DIR/builds/posix" "POSIX build system"
copy_with_exclusions "$SOURCE_DIR/builds/install" "$TARGET_DIR/builds/install" "Installation scripts"

# Copy source code
copy_with_exclusions "$SOURCE_DIR/src" "$TARGET_DIR/src" "Core source code (31MB)"

# Copy external dependencies
echo
echo "📦 Copying external dependencies..."
copy_with_exclusions "$SOURCE_DIR/extern/boost" "$TARGET_DIR/extern/boost" "Boost libraries"
copy_with_exclusions "$SOURCE_DIR/extern/decNumber" "$TARGET_DIR/extern/decNumber" "Decimal number library"
copy_with_exclusions "$SOURCE_DIR/extern/icu" "$TARGET_DIR/extern/icu" "ICU Unicode support"
copy_with_exclusions "$SOURCE_DIR/extern/editline" "$TARGET_DIR/extern/editline" "Command line editing"
copy_with_exclusions "$SOURCE_DIR/extern/libtomcrypt" "$TARGET_DIR/extern/libtomcrypt" "Cryptographic library"
copy_with_exclusions "$SOURCE_DIR/extern/btyacc" "$TARGET_DIR/extern/btyacc" "Parser generator"
copy_with_exclusions "$SOURCE_DIR/extern/cloop" "$TARGET_DIR/extern/cloop" "Code generator"

# Copy documentation
copy_with_exclusions "$SOURCE_DIR/doc" "$TARGET_DIR/doc" "Documentation"

# Copy examples (optional)
copy_with_exclusions "$SOURCE_DIR/examples" "$TARGET_DIR/examples" "Code examples"

echo
echo "✅ Phase 1 Complete: Essential components copied"

# Phase 2: Rename and rebrand files
echo
echo "🔧 Phase 2: Renaming and rebranding..."

# Function to rename files with Firebird references
rename_firebird_files() {
    echo "🏷️  Renaming Firebird references in filenames..."
    
    find "$TARGET_DIR" -name "*firebird*" -type f | while read -r file; do
        local dir=$(dirname "$file")
        local basename=$(basename "$file")
        local newname=$(echo "$basename" | sed 's/firebird/scratchbird/g')
        if [[ "$basename" != "$newname" ]]; then
            echo "  Renaming: $basename → $newname"
            mv "$file" "$dir/$newname"
        fi
    done
    
    find "$TARGET_DIR" -name "*fb*" -type f | while read -r file; do
        local dir=$(dirname "$file")
        local basename=$(basename "$file")
        local newname=$(echo "$basename" | sed 's/fb/sb/g')
        if [[ "$basename" != "$newname" ]]; then
            echo "  Renaming: $basename → $newname"
            mv "$file" "$dir/$newname"
        fi
    done
}

# Function to update content references
update_content_references() {
    echo "📝 Updating content references..."
    
    # Update CMakeLists.txt
    if [[ -f "$TARGET_DIR/CMakeLists.txt" ]]; then
        sed -i 's/Firebird/ScratchBird/g' "$TARGET_DIR/CMakeLists.txt"
        sed -i 's/firebird/scratchbird/g' "$TARGET_DIR/CMakeLists.txt"
        echo "  Updated CMakeLists.txt"
    fi
    
    # Update configure files
    if [[ -f "$TARGET_DIR/configure.ac" ]]; then
        sed -i 's/Firebird/ScratchBird/g' "$TARGET_DIR/configure.ac"
        sed -i 's/firebird/scratchbird/g' "$TARGET_DIR/configure.ac"
        echo "  Updated configure.ac"
    fi
}

# Execute renaming
rename_firebird_files
update_content_references

echo "✅ Phase 2 Complete: Renaming and rebranding done"

# Phase 3: Create ScratchBird-specific files
echo
echo "📝 Phase 3: Creating ScratchBird-specific files..."

# Create main README.md
cat > "$TARGET_DIR/README.md" << 'EOF'
# ScratchBird Database v0.5

ScratchBird is an advanced open-source relational database management system based on Firebird 6.0 with significant enhancements for modern development.

## Key Features

### 🎯 SQL Dialect 4 Enhancements
- **FROM-less SELECT statements**: `SELECT GEN_UUID();`
- **Multi-row INSERT VALUES**: `INSERT INTO table VALUES (1,2,3),(4,5,6);`
- **Comprehensive SYNONYM support**: Schema-aware object aliasing

### 🌳 Hierarchical Schema System
- **8-level deep schema nesting**: `finance.accounting.reports.table`
- **PostgreSQL-style qualified names**: Exceeds PostgreSQL capabilities
- **Schema-aware database links**: Distributed database scenarios

### 🔐 Enhanced Security
- **Trusted authentication**: GSSAPI/Kerberos integration
- **Advanced authentication plugins**: Multi-factor support
- **Schema-level security**: Granular access control

### 🔗 Database Links
- **5 schema resolution modes**: NONE, FIXED, CONTEXT_AWARE, HIERARCHICAL, MIRROR
- **Remote schema targeting**: Seamless distributed queries
- **Context-aware resolution**: CURRENT, HOME, USER schema references

## Tools

All ScratchBird tools are prefixed with `sb_` to avoid conflicts:

- `sb_isql` - Interactive SQL shell with schema commands
- `sb_gbak` - Database backup and restore
- `sb_gfix` - Database repair and validation
- `sb_gsec` - User and security management
- `sb_gstat` - Database statistics and analysis
- `sb_nbackup` - Online backup utility
- `sb_svcmgr` - Service manager
- `sb_tracemgr` - Trace and monitoring

## Quick Start

```bash
# Build ScratchBird
cd builds/cmake
cmake ..
make -j$(nproc)

# Start interactive shell
./src/isql/sb_isql

# Create hierarchical schema
CREATE SCHEMA finance;
CREATE SCHEMA finance.accounting;
CREATE SCHEMA finance.accounting.reports;

# FROM-less SELECT (SQL Dialect 4)
SELECT GEN_UUID();
SELECT CURRENT_TIMESTAMP;

# Multi-row INSERT (SQL Dialect 4)
INSERT INTO mytable(id, name) VALUES (1,'Alice'), (2,'Bob'), (3,'Carol');

# Create synonym
CREATE SYNONYM emp_data FOR hr.employees;
```

## Documentation

- [SQL Dialect 4 Reference](doc/README.sql_dialect_4.md)
- [Hierarchical Schemas Guide](doc/README.hierarchical_schemas.md)
- [Build Instructions](doc/README.build.posix.html)

## License

ScratchBird is released under the Initial Developer's Public License (IDPL).

## Contributing

ScratchBird is an active fork maintaining compatibility with Firebird while adding modern database features.
EOF

# Create CHANGELOG.md
cat > "$TARGET_DIR/CHANGELOG.md" << 'EOF'
# ScratchBird Changelog

## Version 0.5 (2024-01-11)

### 🎯 SQL Dialect 4 Implementation
- **FROM-less SELECT statements**: Enable singleton queries without FROM clause
- **Multi-row INSERT VALUES**: Support multiple value groups in single statement
- **SYNONYM support**: Complete DDL infrastructure with schema-aware targeting

### 🌳 Hierarchical Schema System
- **8-level schema nesting**: Deep hierarchical schema organization
- **Schema navigation**: Path-based schema resolution and navigation
- **PostgreSQL compatibility**: Exceeds PostgreSQL schema capabilities

### 🔗 Database Links Enhancement
- **Schema-aware links**: 5 resolution modes for distributed databases
- **Context resolution**: CURRENT, HOME, USER schema references
- **Performance optimization**: Cached schema depth and path resolution

### 🛠️ Client Tool Updates
- **ISQL enhancements**: SET SCHEMA, SHOW SCHEMA commands
- **GBAK integration**: Hierarchical schema backup/restore
- **Python drivers**: SQL Dialect 4 support and object type constants
- **FlameRobin**: Complete metadata support and tree navigation

### 🏗️ Build System
- **CMake modernization**: Updated build configuration
- **Cross-platform support**: Enhanced POSIX/Linux compatibility
- **Tool renaming**: All tools prefixed with `sb_` (sb_isql, sb_gbak, etc.)

## Version 0.4 (Based on Firebird 6.0.0.929)

### Core Features
- Full Firebird 6.0 feature set
- ACID compliance and transaction management
- Multi-Version Concurrency Control (MVCC)
- Advanced indexing and query optimization
- User-defined functions and procedures
- Triggers and stored procedures
- International character set support

### Security
- SRP (Secure Remote Password) authentication
- Plugin-based authentication architecture
- Database encryption support
- Role-based access control
EOF

# Create installation script
cat > "$TARGET_DIR/builds/install/install_scratchbird.sh" << 'EOF'
#!/bin/bash

# ScratchBird Installation Script
echo "🔥 Installing ScratchBird Database v0.5"

# Set installation directories
INSTALL_PREFIX="/opt/scratchbird"
BIN_DIR="$INSTALL_PREFIX/bin"
LIB_DIR="$INSTALL_PREFIX/lib"
DOC_DIR="$INSTALL_PREFIX/doc"

# Create directories
sudo mkdir -p "$BIN_DIR" "$LIB_DIR" "$DOC_DIR"

# Install binaries
echo "Installing ScratchBird tools..."
sudo cp src/isql/sb_isql "$BIN_DIR/"
sudo cp src/burp/sb_gbak "$BIN_DIR/"
sudo cp src/alice/sb_gfix "$BIN_DIR/"
sudo cp src/utilities/gsec/sb_gsec "$BIN_DIR/"
sudo cp src/utilities/gstat/sb_gstat "$BIN_DIR/"

# Set permissions
sudo chmod +x "$BIN_DIR"/*

# Create symlinks in /usr/local/bin
echo "Creating system-wide symlinks..."
sudo ln -sf "$BIN_DIR/sb_isql" /usr/local/bin/
sudo ln -sf "$BIN_DIR/sb_gbak" /usr/local/bin/
sudo ln -sf "$BIN_DIR/sb_gfix" /usr/local/bin/
sudo ln -sf "$BIN_DIR/sb_gsec" /usr/local/bin/
sudo ln -sf "$BIN_DIR/sb_gstat" /usr/local/bin/

echo "✅ ScratchBird installation complete!"
echo "Run 'sb_isql' to start the interactive SQL shell"
EOF

chmod +x "$TARGET_DIR/builds/install/install_scratchbird.sh"

echo "✅ Phase 3 Complete: ScratchBird-specific files created"

# Phase 4: Summary and statistics
echo
echo "📊 Migration Summary"
echo "==================="

# Calculate sizes
if command -v du >/dev/null 2>&1; then
    TARGET_SIZE=$(du -sh "$TARGET_DIR" | cut -f1)
    echo "📦 ScratchBird directory size: $TARGET_SIZE"
fi

# Count files
if command -v find >/dev/null 2>&1; then
    FILE_COUNT=$(find "$TARGET_DIR" -type f | wc -l)
    DIR_COUNT=$(find "$TARGET_DIR" -type d | wc -l)
    echo "📁 Directories: $DIR_COUNT"
    echo "📄 Files: $FILE_COUNT"
fi

echo
echo "🎉 ScratchBird Migration Complete!"
echo
echo "Next Steps:"
echo "1. cd $TARGET_DIR"
echo "2. Review and test the build system"
echo "3. Run: builds/install/install_scratchbird.sh"
echo "4. Test SQL Dialect 4 features"
echo
echo "🔥 Welcome to ScratchBird v0.5!"