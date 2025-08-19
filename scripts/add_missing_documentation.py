#!/usr/bin/env python3
"""
ScratchBird Missing Documentation Addition Script
Adds critical missing ScratchBird-specific features to the documentation

Usage: python3 add_missing_documentation.py
"""

import os
import re
import sys
from pathlib import Path
from bs4 import BeautifulSoup
from datetime import datetime

class MissingDocumentationAdder:
    def __init__(self, target_doc_dir):
        self.target_doc_dir = Path(target_doc_dir)
        
        # Version correction: ScratchBird will be 1.0 when it goes gold
        self.version_updates = {
            r'ScratchBird 6\.0': 'ScratchBird 1.0',
            r'ScratchBird v6\.0': 'ScratchBird v1.0',
            r'ScratchBird 0\.6\.0': 'ScratchBird 1.0',
            r'v6\.0\.0': 'v1.0.0',
            r'Version 6\.0': 'Version 1.0',
        }
        
    def add_unsigned_integer_documentation(self):
        """Add comprehensive unsigned integer type documentation"""
        print("📊 Adding unsigned integer documentation...")
        
        unsigned_int_content = """
        <section class="sb-unsigned-integers" id="unsigned-integer-types">
            <h2>Unsigned Integer Types</h2>
            <div class="sb-new-feature">
                <p><strong>ScratchBird Enhancement:</strong> Full support for unsigned integer types with extended range capabilities.</p>
            </div>
            
            <p>ScratchBird extends standard SQL with comprehensive unsigned integer support, providing double the positive range compared to signed equivalents:</p>
            
            <table class="sb-datatype-table">
                <thead>
                    <tr>
                        <th>Type</th>
                        <th>Size (bytes)</th>
                        <th>Range</th>
                        <th>Equivalent Signed</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td><code>USHORT</code></td>
                        <td>2</td>
                        <td>0 to 65,535</td>
                        <td>SMALLINT (-32,768 to 32,767)</td>
                    </tr>
                    <tr>
                        <td><code>ULONG</code></td>
                        <td>4</td>
                        <td>0 to 4,294,967,295</td>
                        <td>INTEGER (-2,147,483,648 to 2,147,483,647)</td>
                    </tr>
                    <tr>
                        <td><code>UINT64</code></td>
                        <td>8</td>
                        <td>0 to 18,446,744,073,709,551,615</td>
                        <td>BIGINT (-9,223,372,036,854,775,808 to 9,223,372,036,854,775,807)</td>
                    </tr>
                    <tr>
                        <td><code>UINT128</code></td>
                        <td>16</td>
                        <td>0 to 340,282,366,920,938,463,463,374,607,431,768,211,455</td>
                        <td>INT128 (signed 128-bit range)</td>
                    </tr>
                </tbody>
            </table>
            
            <h3>Usage Examples</h3>
            <pre><code>-- Create table with unsigned integer columns
CREATE TABLE statistics (
    id UINT64,                          -- Large positive IDs
    user_count ULONG,                   -- User counts up to 4+ billion
    event_timestamp UINT64,             -- Unix timestamps in milliseconds
    hash_value UINT128                  -- Large hash values
);

-- Insert data with large unsigned values
INSERT INTO statistics VALUES (
    18000000000000000000,               -- Large UINT64 ID
    3000000000,                         -- 3 billion users
    1640995200000,                      -- Timestamp in milliseconds
    123456789012345678901234567890123456789  -- 128-bit hash
);

-- Queries with unsigned arithmetic
SELECT 
    id + 1000000000000000000 AS next_id,
    user_count * 2 AS doubled_users,
    hash_value & 0xFFFFFFFFFFFFFFFF AS lower_64_bits
FROM statistics;</code></pre>
            
            <h3>Arithmetic Operations</h3>
            <p>Unsigned integers support all standard arithmetic operations with overflow protection:</p>
            <ul>
                <li><strong>Addition/Subtraction</strong>: Automatic overflow detection</li>
                <li><strong>Multiplication/Division</strong>: Result promotion to larger type if needed</li>
                <li><strong>Bitwise Operations</strong>: AND, OR, XOR, shift operations fully supported</li>
                <li><strong>Comparison</strong>: Proper unsigned comparison semantics</li>
            </ul>
            
            <h3>Index Support</h3>
            <p>All unsigned integer types support:</p>
            <ul>
                <li>Primary key constraints</li>
                <li>Unique constraints</li>
                <li>Foreign key relationships</li>
                <li>B-tree and hash indexes</li>
                <li>Ascending and descending indexes</li>
            </ul>
            
            <h3>Conversion and Compatibility</h3>
            <pre><code>-- Safe conversions (no data loss)
SELECT CAST(32000 AS USHORT);           -- SMALLINT to USHORT
SELECT CAST(user_id AS UINT64);         -- Promote to larger unsigned type

-- Explicit conversions (potential data loss)
SELECT CAST(large_value AS ULONG);      -- UINT64 to ULONG (check bounds)

-- Mixed signed/unsigned arithmetic
SELECT signed_col + unsigned_col;       -- Result is promoted to larger type</code></pre>
        </section>
        """
        
        return self.insert_content_into_section(
            '02-datatypes.html', 
            unsigned_int_content, 
            'Integer Types'
        )
        
    def add_page_size_documentation(self):
        """Add 128 page size configuration documentation"""
        print("📄 Adding page size configuration documentation...")
        
        page_size_content = """
        <section class="sb-page-size-config" id="page-size-configuration">
            <h2>Database Page Size Configuration</h2>
            <div class="sb-new-feature">
                <p><strong>ScratchBird Enhancement:</strong> Support for 128-byte page size for specialized applications.</p>
            </div>
            
            <p>ScratchBird extends the standard page size options to include ultra-small page sizes for specific use cases:</p>
            
            <table class="sb-page-size-table">
                <thead>
                    <tr>
                        <th>Page Size</th>
                        <th>Use Case</th>
                        <th>Max String Length Formula</th>
                        <th>Example Max CHAR Length</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td><code>128</code></td>
                        <td>IoT devices, embedded systems</td>
                        <td><code>FLOOR((128 / 4 - 9) / N)</code></td>
                        <td>23 characters</td>
                    </tr>
                    <tr>
                        <td><code>1024</code></td>
                        <td>Small applications</td>
                        <td><code>FLOOR((1024 / 4 - 9) / N)</code></td>
                        <td>246 characters</td>
                    </tr>
                    <tr>
                        <td><code>4096</code></td>
                        <td>Standard applications</td>
                        <td><code>FLOOR((4096 / 4 - 9) / N)</code></td>
                        <td>1015 characters</td>
                    </tr>
                    <tr>
                        <td><code>8192</code></td>
                        <td>Large applications</td>
                        <td><code>FLOOR((8192 / 4 - 9) / N)</code></td>
                        <td>2039 characters</td>
                    </tr>
                    <tr>
                        <td><code>16384</code></td>
                        <td>High-performance applications</td>
                        <td><code>FLOOR((16384 / 4 - 9) / N)</code></td>
                        <td>4087 characters</td>
                    </tr>
                    <tr>
                        <td><code>32768</code></td>
                        <td>Maximum performance</td>
                        <td><code>FLOOR((32768 / 4 - 9) / N)</code></td>
                        <td>8183 characters</td>
                    </tr>
                </tbody>
            </table>
            
            <h3>Page Size Selection Guidelines</h3>
            <div class="sb-guidelines">
                <h4>128-byte Pages</h4>
                <p><strong>Best for:</strong></p>
                <ul>
                    <li>IoT sensors with minimal memory</li>
                    <li>Embedded systems with tight constraints</li>
                    <li>Cache-optimized micro-databases</li>
                    <li>Real-time systems requiring minimal latency</li>
                </ul>
                <p><strong>Limitations:</strong></p>
                <ul>
                    <li>Very small maximum string lengths</li>
                    <li>Limited records per page</li>
                    <li>Higher metadata overhead ratio</li>
                </ul>
                
                <h4>Standard Pages (4KB-32KB)</h4>
                <p><strong>Best for:</strong></p>
                <ul>
                    <li>General business applications</li>
                    <li>Web applications</li>
                    <li>Data warehousing</li>
                    <li>High-volume OLTP systems</li>
                </ul>
            </div>
            
            <h3>Database Creation with Custom Page Size</h3>
            <pre><code>-- Create database with 128-byte pages (IoT/embedded)
CREATE DATABASE 'iot_sensors.sdb'
PAGE_SIZE 128
DEFAULT CHARACTER SET UTF8;

-- Create database with standard 8KB pages
CREATE DATABASE 'business_app.sdb'
PAGE_SIZE 8192
DEFAULT CHARACTER SET UTF8;

-- Create database with maximum 32KB pages (performance)
CREATE DATABASE 'warehouse.sdb'
PAGE_SIZE 32768
DEFAULT CHARACTER SET UTF8;</code></pre>
            
            <h3>String Length Calculations</h3>
            <p>The maximum character length formula accounts for page overhead:</p>
            <pre><code>-- General formula for maximum CHAR length
max_char_length = FLOOR((page_size / 4 - 9) / N)

-- Where:
-- page_size = Database page size in bytes
-- N = Character set multiplier (1 for ASCII, 3 for UTF8, 4 for UTF8MB4)
-- 4 = Bytes per character allocation unit
-- 9 = Page header overhead (approximate)

-- Examples:
-- 128-byte page, ASCII: FLOOR((128/4 - 9) / 1) = 23 chars
-- 128-byte page, UTF8:  FLOOR((128/4 - 9) / 3) = 7 chars
-- 4096-byte page, UTF8: FLOOR((4096/4 - 9) / 3) = 338 chars</code></pre>
            
            <h3>Performance Considerations</h3>
            <table class="sb-performance-table">
                <thead>
                    <tr>
                        <th>Page Size</th>
                        <th>I/O Characteristics</th>
                        <th>Memory Usage</th>
                        <th>Best Use Case</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>128 bytes</td>
                        <td>Minimal I/O, high frequency</td>
                        <td>Ultra-low memory</td>
                        <td>Real-time embedded systems</td>
                    </tr>
                    <tr>
                        <td>4-8 KB</td>
                        <td>Balanced I/O efficiency</td>
                        <td>Moderate memory usage</td>
                        <td>General business applications</td>
                    </tr>
                    <tr>
                        <td>16-32 KB</td>
                        <td>High throughput, larger I/O</td>
                        <td>Higher memory usage</td>
                        <td>Data warehousing, analytics</td>
                    </tr>
                </tbody>
            </table>
        </section>
        """
        
        return self.insert_content_into_section(
            '09-administration.html',
            page_size_content,
            'Database Configuration'
        )
        
    def add_sql_dialect_4_documentation(self):
        """Add comprehensive SQL Dialect 4 documentation"""
        print("🗣️ Adding SQL Dialect 4 documentation...")
        
        dialect_4_content = """
        <section class="sb-sql-dialect-4" id="sql-dialect-4">
            <h1>SQL Dialect 4 - ScratchBird Extended Syntax</h1>
            <div class="sb-new-feature">
                <p><strong>ScratchBird Innovation:</strong> SQL Dialect 4 introduces advanced features for modern database applications.</p>
            </div>
            
            <p>SQL Dialect 4 is ScratchBird's extended SQL syntax mode, providing advanced features while maintaining backward compatibility with Dialect 3.</p>
            
            <h2>Dialect Comparison</h2>
            <table class="sb-dialect-comparison">
                <thead>
                    <tr>
                        <th>Feature</th>
                        <th>Dialect 1</th>
                        <th>Dialect 3</th>
                        <th>Dialect 4 (ScratchBird)</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>Date/Time Handling</td>
                        <td>Legacy format</td>
                        <td>Standard SQL</td>
                        <td>Enhanced with microseconds</td>
                    </tr>
                    <tr>
                        <td>String Operations</td>
                        <td>Basic concatenation</td>
                        <td>Standard SQL</td>
                        <td>PostgreSQL-compatible operators</td>
                    </tr>
                    <tr>
                        <td>Numeric Precision</td>
                        <td>Limited</td>
                        <td>Up to 18 digits</td>
                        <td>128-bit integers, enhanced precision</td>
                    </tr>
                    <tr>
                        <td>Array Support</td>
                        <td>Basic arrays</td>
                        <td>Standard arrays</td>
                        <td>PostgreSQL-compatible array operators</td>
                    </tr>
                    <tr>
                        <td>Schema Support</td>
                        <td>None</td>
                        <td>Flat schemas</td>
                        <td>8-level hierarchical schemas</td>
                    </tr>
                    <tr>
                        <td>Vector Types</td>
                        <td>None</td>
                        <td>None</td>
                        <td>VECTOR datatype for AI/ML</td>
                    </tr>
                    <tr>
                        <td>Geometric Types</td>
                        <td>None</td>
                        <td>None</td>
                        <td>POINT and spatial operations</td>
                    </tr>
                </tbody>
            </table>
            
            <h2>Dialect 4 Exclusive Features</h2>
            
            <h3>Hierarchical Schema Syntax</h3>
            <pre><code>-- Set default dialect to 4
SET SQL DIALECT 4;

-- Create nested schema structures
CREATE SCHEMA company.finance.accounting;
CREATE TABLE company.finance.accounting.transactions (
    id UINT64,
    amount DECIMAL(15,2),
    transaction_date TIMESTAMP
);

-- Schema-aware object references
SELECT * FROM company.finance.accounting.transactions
WHERE transaction_date > CURRENT_TIMESTAMP - INTERVAL '30 DAYS';</code></pre>
            
            <h3>Enhanced Vector Operations</h3>
            <pre><code>-- Vector similarity search with Dialect 4 syntax
CREATE TABLE embeddings (
    id UINT64,
    content_vector VECTOR(1536),
    metadata JSONB
);

-- Advanced vector queries
SELECT id, content_vector <-> '[0.1, 0.2, ...]'::VECTOR AS distance
FROM embeddings
ORDER BY distance
LIMIT 10;</code></pre>
            
            <h3>Advanced Array Operations</h3>
            <pre><code>-- PostgreSQL-compatible array operators in Dialect 4
SELECT product_id 
FROM products 
WHERE tags @> ARRAY['electronics', 'wireless']    -- Contains operator
   AND categories <@ ARRAY['tech', 'gadgets', 'mobile']  -- Contained by
   AND features && ARRAY['bluetooth', 'usb-c'];    -- Overlaps operator</code></pre>
            
            <h3>Enhanced Data Types</h3>
            <pre><code>-- Unsigned integer types
CREATE TABLE counters (
    page_views UINT64,
    user_count ULONG,
    session_id UINT128
);

-- Network address types
CREATE TABLE network_log (
    client_ip INET,
    subnet CIDR,
    mac_address MACADDR,
    logged_at TIMESTAMP
);

-- Range types
CREATE TABLE reservations (
    room_id INTEGER,
    date_range DATERANGE,
    time_slot TSRANGE,
    price_range NUMRANGE
);</code></pre>
            
            <h3>Advanced Full-Text Search</h3>
            <pre><code>-- TSVECTOR and TSQUERY with ranking
SELECT document_id,
       ts_rank_cd(document_vector, query) AS relevance
FROM documents, 
     to_tsquery('english', 'database & search') AS query
WHERE document_vector @@ query
ORDER BY relevance DESC;</code></pre>
            
            <h2>Dialect Configuration</h2>
            
            <h3>Database-Level Dialect</h3>
            <pre><code>-- Create database with Dialect 4
CREATE DATABASE 'modern_app.sdb'
SQL DIALECT 4
PAGE_SIZE 8192
DEFAULT CHARACTER SET UTF8;

-- Alter existing database to Dialect 4 (requires exclusive access)
ALTER DATABASE SET SQL DIALECT 4;</code></pre>
            
            <h3>Connection-Level Dialect</h3>
            <pre><code>-- Set dialect for current connection
SET SQL DIALECT 4;

-- Check current dialect
SELECT RDB$GET_CONTEXT('SYSTEM', 'SQL_DIALECT') FROM RDB$DATABASE;</code></pre>
            
            <h3>Application Integration</h3>
            <pre><code>// JDBC connection string with Dialect 4
String url = "jdbc:scratchbird://localhost:3050/mydb?sql_dialect=4&encoding=UTF8";

// .NET connection string
string connString = "Server=localhost;Database=mydb;User=sysdba;Password=pass;Dialect=4";

// Python connection
import scratchbird
conn = scratchbird.connect(
    dsn='localhost:mydb',
    user='sysdba',
    password='masterpass',
    sql_dialect=4
)</code></pre>
            
            <h2>Backward Compatibility</h2>
            <div class="sb-compatibility-info">
                <h3>✅ Fully Compatible</h3>
                <ul>
                    <li>All Dialect 3 SQL statements work unchanged</li>
                    <li>Existing stored procedures and functions</li>
                    <li>All standard data types and operations</li>
                    <li>Client applications using Dialect 3</li>
                </ul>
                
                <h3>🔄 Enhanced in Dialect 4</h3>
                <ul>
                    <li>Schema resolution with hierarchical paths</li>
                    <li>Extended data type support (unsigned, vector, network)</li>
                    <li>Enhanced array and text search operations</li>
                    <li>Advanced mathematical and geometric functions</li>
                </ul>
                
                <h3>⚠️ Dialect 4 Only</h3>
                <ul>
                    <li>Hierarchical schema creation and references</li>
                    <li>VECTOR and POINT data types</li>
                    <li>Unsigned integer types (USHORT, ULONG, UINT64, UINT128)</li>
                    <li>Network address types (INET, CIDR, MACADDR)</li>
                    <li>PostgreSQL-compatible array operators (@>, <@, &&)</li>
                </ul>
            </div>
            
            <h2>Migration to Dialect 4</h2>
            
            <h3>Pre-Migration Checklist</h3>
            <ol>
                <li><strong>Backup Database</strong>: Full backup before dialect change</li>
                <li><strong>Test Applications</strong>: Verify compatibility with existing code</li>
                <li><strong>Review Schema Design</strong>: Plan hierarchical schema organization</li>
                <li><strong>Update Connection Strings</strong>: Add dialect=4 parameter</li>
                <li><strong>Training</strong>: Educate developers on new features</li>
            </ol>
            
            <h3>Step-by-Step Migration</h3>
            <pre><code>-- 1. Create backup
sb_gbak -b -user sysdba -password masterpass mydb.sdb mydb_backup.sbk

-- 2. Set database to exclusive mode
sb_gfix -shut single -attach 30 -user sysdba -password masterpass mydb.sdb

-- 3. Upgrade to Dialect 4
ALTER DATABASE SET SQL DIALECT 4;

-- 4. Return to normal mode
sb_gfix -online -user sysdba -password masterpass mydb.sdb

-- 5. Verify dialect
SELECT RDB$GET_CONTEXT('SYSTEM', 'SQL_DIALECT') FROM RDB$DATABASE;</code></pre>
            
            <h2>Best Practices</h2>
            <div class="sb-best-practices">
                <h3>Schema Organization</h3>
                <ul>
                    <li>Use hierarchical schemas to organize related objects</li>
                    <li>Limit nesting to 4-5 levels for maintainability</li>
                    <li>Establish naming conventions for schema paths</li>
                    <li>Document schema hierarchies for team members</li>
                </ul>
                
                <h3>Performance Optimization</h3>
                <ul>
                    <li>Use unsigned integers for large positive values</li>
                    <li>Leverage vector indexing for similarity searches</li>
                    <li>Optimize array queries with proper indexing</li>
                    <li>Use appropriate page sizes for workload characteristics</li>
                </ul>
                
                <h3>Application Development</h3>
                <ul>
                    <li>Set dialect in connection strings explicitly</li>
                    <li>Test new features in development environment first</li>
                    <li>Use parameter binding for vector and array data</li>
                    <li>Implement proper error handling for dialect-specific features</li>
                </ul>
            </div>
        </section>
        """
        
        return self.insert_content_into_section(
            '03-language-elements.html',
            dialect_4_content,
            'SQL Dialects'
        )
        
    def update_version_references(self):
        """Update all version references from 6.0 to 1.0"""
        print("🔄 Updating version references to ScratchBird 1.0...")
        
        updated_files = 0
        
        for html_file in self.target_doc_dir.glob('*.html'):
            try:
                with open(html_file, 'r', encoding='utf-8') as f:
                    content = f.read()
                    
                original_content = content
                
                # Apply version updates
                for pattern, replacement in self.version_updates.items():
                    content = re.sub(pattern, replacement, content)
                    
                # Only write if changes were made
                if content != original_content:
                    with open(html_file, 'w', encoding='utf-8') as f:
                        f.write(content)
                    updated_files += 1
                    
            except Exception as e:
                print(f"❌ Error updating {html_file}: {e}")
                
        print(f"✅ Updated version references in {updated_files} files")
        return updated_files
        
    def add_scratchbird_specific_config(self):
        """Add ScratchBird-specific configuration documentation"""
        print("⚙️ Adding ScratchBird configuration documentation...")
        
        config_content = """
        <section class="sb-config-reference" id="scratchbird-configuration">
            <h2>ScratchBird Configuration Reference</h2>
            <div class="sb-new-feature">
                <p><strong>ScratchBird Enhancements:</strong> Extended configuration options for advanced features.</p>
            </div>
            
            <h3>scratchbird.conf Extensions</h3>
            <p>ScratchBird extends the standard configuration with the following additional parameters:</p>
            
            <pre><code># Vector and AI/ML Configuration
VectorIndexCacheSize = 64MB        # Cache size for vector indexes
VectorSimilarityThreshold = 0.8    # Default similarity threshold
EnableVectorOptimizations = true   # Enable vector query optimizations

# Hierarchical Schema Configuration  
MaxSchemaDepth = 8                 # Maximum schema nesting levels (1-8)
SchemaPathCacheSize = 16MB         # Cache for schema path resolution
EnableSchemaInheritance = true     # Allow schema-level permissions inheritance

# Database Links Configuration
MaxDatabaseLinks = 50              # Maximum concurrent database links
DatabaseLinkTimeout = 30           # Link connection timeout (seconds)
EnableSchemaAwareLinks = true      # Enable schema-aware database links

# Page Size Configuration
MinPageSize = 128                  # Allow 128-byte pages for embedded systems
MaxPageSize = 32768                # Standard maximum page size

# Unsigned Integer Support
EnableUnsignedIntegers = true      # Enable USHORT, ULONG, UINT64, UINT128
UnsignedIntegerChecking = strict   # Overflow checking: strict, permissive

# Network Type Configuration
EnableNetworkTypes = true          # Enable INET, CIDR, MACADDR types
NetworkTypeIndexing = true         # Enable specialized network indexing

# SQL Dialect 4 Configuration
DefaultSQLDialect = 4              # Set default dialect for new databases
AllowDialectUpgrade = true         # Allow automatic dialect upgrades
DialogMixingMode = permissive      # strict, permissive for dialect mixing

# Performance Optimizations
EnableVectorSIMD = true            # Use SIMD instructions for vectors
EnableSchemaQueryCache = true      # Cache schema-qualified queries
OptimizeHierarchicalQueries = true # Optimize nested schema queries</code></pre>
            
            <h3>Environment Variables</h3>
            <table class="sb-env-vars">
                <thead>
                    <tr>
                        <th>Variable</th>
                        <th>Purpose</th>
                        <th>Default</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td><code>SCRATCHBIRD_HOME</code></td>
                        <td>ScratchBird installation directory</td>
                        <td>/opt/scratchbird</td>
                    </tr>
                    <tr>
                        <td><code>SCRATCHBIRD_CONF</code></td>
                        <td>Configuration file location</td>
                        <td>$SCRATCHBIRD_HOME/scratchbird.conf</td>
                    </tr>
                    <tr>
                        <td><code>SCRATCHBIRD_MSG</code></td>
                        <td>Message file location</td>
                        <td>$SCRATCHBIRD_HOME/scratchbird.msg</td>
                    </tr>
                    <tr>
                        <td><code>SCRATCHBIRD_TMP</code></td>
                        <td>Temporary directory</td>
                        <td>/tmp</td>
                    </tr>
                </tbody>
            </table>
            
            <h3>Client Library Configuration</h3>
            <pre><code>// C/C++ Application Configuration
#include <scratchbird.h>

// Enable Dialect 4 features
sb_dpb_add_string(&dpb, sb_dpb_sql_dialect, "4");

// Configure vector operations
sb_dpb_add_string(&dpb, sb_dpb_vector_cache_size, "32MB");

// Enable schema-aware connections
sb_dpb_add_string(&dpb, sb_dpb_schema_mode, "hierarchical");</code></pre>
            
            <h3>Performance Tuning</h3>
            <div class="sb-performance-tuning">
                <h4>Vector Operations</h4>
                <ul>
                    <li><strong>VectorIndexCacheSize</strong>: Increase for applications with many vector searches</li>
                    <li><strong>EnableVectorSIMD</strong>: Enables hardware acceleration for vector calculations</li>
                    <li><strong>VectorSimilarityThreshold</strong>: Adjust based on application requirements</li>
                </ul>
                
                <h4>Schema Operations</h4>
                <ul>
                    <li><strong>SchemaPathCacheSize</strong>: Increase for databases with deep schema hierarchies</li>
                    <li><strong>MaxSchemaDepth</strong>: Reduce if not using deep nesting (saves memory)</li>
                    <li><strong>EnableSchemaQueryCache</strong>: Improves performance for repeated schema-qualified queries</li>
                </ul>
                
                <h4>Database Links</h4>
                <ul>
                    <li><strong>MaxDatabaseLinks</strong>: Set based on expected concurrent remote connections</li>
                    <li><strong>DatabaseLinkTimeout</strong>: Adjust for network conditions</li>
                    <li><strong>EnableSchemaAwareLinks</strong>: Disable if not using schema mapping</li>
                </ul>
            </div>
        </section>
        """
        
        return self.insert_content_into_section(
            '09-administration.html',
            config_content,
            'Configuration Files'
        )
        
    def insert_content_into_section(self, html_file, content, section_identifier):
        """Insert content into HTML file at appropriate location"""
        file_path = self.target_doc_dir / html_file
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                html_content = f.read()
                
            # Insert content before the closing main tag or before scripts
            insertion_point = html_content.rfind('</main>')
            if insertion_point == -1:
                insertion_point = html_content.rfind('<script')
            if insertion_point == -1:
                insertion_point = html_content.rfind('</body>')
                
            if insertion_point != -1:
                new_content = (
                    html_content[:insertion_point] + 
                    content + 
                    "\n\n" + 
                    html_content[insertion_point:]
                )
                
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(new_content)
                    
                print(f"✅ Added content to {html_file}")
                return True
            else:
                print(f"❌ Could not find insertion point in {html_file}")
                return False
                
        except Exception as e:
            print(f"❌ Error updating {html_file}: {e}")
            return False
            
    def run(self):
        """Execute the missing documentation addition process"""
        print("🚀 Adding missing ScratchBird documentation...")
        
        success_count = 0
        
        # Add unsigned integer documentation
        if self.add_unsigned_integer_documentation():
            success_count += 1
            
        # Add page size documentation
        if self.add_page_size_documentation():
            success_count += 1
            
        # Add SQL Dialect 4 documentation
        if self.add_sql_dialect_4_documentation():
            success_count += 1
            
        # Add ScratchBird configuration
        if self.add_scratchbird_specific_config():
            success_count += 1
            
        # Update version references
        version_updates = self.update_version_references()
        
        print(f"\n🎉 Missing documentation addition complete!")
        print(f"   📊 Sections added: {success_count}")
        print(f"   🔄 Files updated for version: {version_updates}")
        print(f"   📁 Documentation location: {self.target_doc_dir}")
        
        return success_count > 0


def main():
    target_doc_dir = "/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/doc/v0.6.0/split_documentation"
    
    if not Path(target_doc_dir).exists():
        print(f"❌ Documentation directory not found: {target_doc_dir}")
        return 1
        
    adder = MissingDocumentationAdder(target_doc_dir)
    success = adder.run()
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())