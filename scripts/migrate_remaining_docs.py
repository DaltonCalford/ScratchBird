#!/usr/bin/env python3
"""
ScratchBird Documentation Migration Script
Processes remaining documentation files for integration into unified documentation structure

Usage: python3 migrate_remaining_docs.py
"""

import os
import re
import sys
from pathlib import Path
import markdown
from datetime import datetime

class DocumentationMigrator:
    def __init__(self, source_doc_dir, target_doc_dir):
        self.source_doc_dir = Path(source_doc_dir)
        self.target_doc_dir = Path(target_doc_dir)
        
        # High-priority files for immediate integration
        self.high_priority_files = {
            'ScratchBird Built-in Functions and Expressions.md': '07-security-functions.html',
            'ScratchBird Data Types_ A Deep Dive.md': '02-datatypes.html',
            'ScratchBird Window Functions.md': '04-data-manipulation.html',
            'ScratchBird PSQL Control-Flow Statements.md': '05-transactions-psql.html',
            'ScratchBird User Management Syntax.md': '07-security-functions.html',
        }
        
        # SQL Extensions that need processing
        self.sql_extensions_map = {
            'README.schemas.md': '06-data-definition.html',
            'README.window_functions.md': '04-data-manipulation.html',
            'README.packages.txt': '05-transactions-psql.html',
            'README.builtin_functions.txt': '07-security-functions.html',
            'README.monitoring_tables': '09-administration.html',
            'README.security_database.txt': '07-security-functions.html',
            'README.performance_monitoring': '09-administration.html',
        }
        
        # Utility documentation files
        self.utility_files = {
            'README.sb_cancel_operation': 'sb_cancel_operation',
            'README.sb_shutdown': 'sb_shutdown', 
            'README.sbsvcmgr': 'sb_svcmgr',
            'README.gbak': 'sb_gbak',
            'README.user.troubleshooting': 'troubleshooting',
        }
        
        # Rebranding mappings
        self.rebrand_map = {
            r'\bFirebird\b': 'ScratchBird',
            r'\bfirebird\b': 'scratchbird',
            r'Firebird ([0-9]\.[0-9])': r'ScratchBird \1',
            r'firebird\.conf': 'scratchbird.conf',
            r'libfbclient': 'libsbclient',
            r'fbclient': 'sbclient',
            r'fbtrace': 'sb_trace',
            r'fbsvcmgr': 'sb_svcmgr',
            r'firebirdsql\.org': 'scratchbird.org',
            r'Firebird Project': 'ScratchBird Project',
            r'\.fdb': '.sdb',  # ScratchBird database extension
            r'\bgbak\b': 'sb_gbak',
            r'\bgstat\b': 'sb_gstat',
            r'\bgsec\b': 'sb_gsec',
            r'\bgfix\b': 'sb_gfix',
            r'\bisql\b': 'sb_isql',
            r'\bgpre\b': 'sb_gpre',
            r'\bnbackup\b': 'sb_nbackup',
        }
        
    def apply_rebranding(self, content):
        """Apply comprehensive rebranding"""
        for pattern, replacement in self.rebrand_map.items():
            content = re.sub(pattern, replacement, content, flags=re.IGNORECASE)
        return content
        
    def process_file_content(self, file_path):
        """Read and process a documentation file"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
            # Apply rebranding
            content = self.apply_rebranding(content)
            
            # Detect format and convert if needed
            if file_path.suffix.lower() == '.md':
                html_content = markdown.markdown(content, extensions=[
                    'toc', 'codehilite', 'tables', 'fenced_code'
                ])
            else:
                # Plain text to HTML
                content = content.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
                html_content = f"<pre class='sb-plaintext'>{content}</pre>"
                
            return html_content
            
        except Exception as e:
            print(f"❌ Error processing {file_path}: {e}")
            return None
            
    def integrate_high_priority_files(self):
        """Integrate high-priority ScratchBird documentation"""
        print("🔄 Processing high-priority documentation files...")
        
        integrated_count = 0
        
        for filename, target_html in self.high_priority_files.items():
            source_file = self.source_doc_dir / filename
            target_file = self.target_doc_dir / target_html
            
            if not source_file.exists():
                print(f"⚠️  File not found: {filename}")
                continue
                
            content = self.process_file_content(source_file)
            if content:
                success = self.insert_content_into_html(
                    target_file, content, filename.replace('.md', '').replace('ScratchBird ', '')
                )
                if success:
                    integrated_count += 1
                    
        print(f"✅ Integrated {integrated_count} high-priority files")
        return integrated_count
        
    def process_sql_extensions(self):
        """Process SQL extensions documentation"""
        print("🔄 Processing SQL extensions documentation...")
        
        sql_ext_dir = self.source_doc_dir / 'sql.extensions'
        if not sql_ext_dir.exists():
            print("⚠️  SQL extensions directory not found")
            return 0
            
        processed_count = 0
        
        for filename, target_html in self.sql_extensions_map.items():
            source_file = sql_ext_dir / filename
            target_file = self.target_doc_dir / target_html
            
            if not source_file.exists():
                continue
                
            content = self.process_file_content(source_file)
            if content:
                success = self.insert_content_into_html(
                    target_file, content, f"SQL Extension: {filename.replace('README.', '').replace('.txt', '').replace('.md', '')}"
                )
                if success:
                    processed_count += 1
                    
        print(f"✅ Processed {processed_count} SQL extension files")
        return processed_count
        
    def create_utilities_section(self):
        """Create dedicated utilities documentation section"""
        print("🔧 Creating utilities documentation section...")
        
        utilities_html = self.create_utilities_html_page()
        
        # Integrate utility documentation
        utility_count = 0
        utility_content_sections = []
        
        for filename, utility_name in self.utility_files.items():
            source_file = self.source_doc_dir / filename
            
            if source_file.exists():
                content = self.process_file_content(source_file)
                if content:
                    utility_content_sections.append({
                        'name': utility_name,
                        'content': content
                    })
                    utility_count += 1
                    
        # Add sections to utilities HTML
        if utility_content_sections:
            self.finalize_utilities_page(utilities_html, utility_content_sections)
            
        print(f"✅ Created utilities section with {utility_count} tools")
        return utility_count
        
    def create_utilities_html_page(self):
        """Create the utilities HTML page structure"""
        utilities_html = f'''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ScratchBird Utilities - ScratchBird 6.0 Language Reference</title>
    <link rel="stylesheet" href="../assets/scratchbird-docs.css">
</head>
<body>
    <nav class="sb-nav">
        <a href="index.html">← Back to Documentation Index</a>
        <div class="sb-version">ScratchBird v6.0</div>
    </nav>
    
    <main class="sb-content">
        <h1>ScratchBird Utility Tools</h1>
        <p>Complete reference for all ScratchBird command-line utilities and administration tools.</p>
        
        <section class="sb-utilities-overview">
            <h2>Utility Tools Overview</h2>
            <table class="sb-utilities-table">
                <thead>
                    <tr>
                        <th>Tool</th>
                        <th>Purpose</th>
                        <th>Description</th>
                    </tr>
                </thead>
                <tbody>
                    <tr><td><code>sb_gbak</code></td><td>Backup/Restore</td><td>Database backup and restore operations</td></tr>
                    <tr><td><code>sb_gstat</code></td><td>Statistics</td><td>Database analysis and statistics</td></tr>
                    <tr><td><code>sb_gfix</code></td><td>Maintenance</td><td>Database repair and maintenance</td></tr>
                    <tr><td><code>sb_gsec</code></td><td>Security</td><td>User and security management</td></tr>
                    <tr><td><code>sb_isql</code></td><td>Interactive SQL</td><td>Command-line SQL interface</td></tr>
                    <tr><td><code>sb_gpre</code></td><td>Preprocessor</td><td>Embedded SQL preprocessor</td></tr>
                    <tr><td><code>sb_svcmgr</code></td><td>Service Manager</td><td>Database service management</td></tr>
                </tbody>
            </table>
        </section>
        
        <div id="utility-sections">
            <!-- Utility sections will be inserted here -->
        </div>
    </main>
    
    <script src="../assets/scratchbird-docs.js"></script>
</body>
</html>'''

        utilities_file = self.target_doc_dir / '11-utilities.html'
        with open(utilities_file, 'w', encoding='utf-8') as f:
            f.write(utilities_html)
            
        return utilities_file
        
    def finalize_utilities_page(self, utilities_file, utility_sections):
        """Add utility content sections to the utilities page"""
        try:
            with open(utilities_file, 'r', encoding='utf-8') as f:
                content = f.read()
                
            sections_html = ""
            for section in utility_sections:
                sections_html += f'''
                <section class="sb-utility-section" id="{section['name']}">
                    <h2>{section['name'].upper()}</h2>
                    <div class="sb-utility-content">
                        {section['content']}
                    </div>
                </section>
                '''
                
            # Insert sections
            content = content.replace('<!-- Utility sections will be inserted here -->', sections_html)
            
            with open(utilities_file, 'w', encoding='utf-8') as f:
                f.write(content)
                
        except Exception as e:
            print(f"❌ Error finalizing utilities page: {e}")
            
    def create_migration_guide(self):
        """Create Firebird to ScratchBird migration guide"""
        print("📋 Creating migration guide...")
        
        migration_content = f'''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Firebird to ScratchBird Migration Guide - ScratchBird 6.0</title>
    <link rel="stylesheet" href="../assets/scratchbird-docs.css">
</head>
<body>
    <nav class="sb-nav">
        <a href="index.html">← Back to Documentation Index</a>
        <div class="sb-version">ScratchBird v6.0</div>
    </nav>
    
    <main class="sb-content">
        <h1>Firebird to ScratchBird Migration Guide</h1>
        <p>Complete guide for migrating from Firebird to ScratchBird 6.0 with new features and enhanced capabilities.</p>
        
        <section class="sb-migration-overview">
            <h2>Migration Overview</h2>
            <div class="sb-new-feature">
                <p>ScratchBird 6.0 maintains full backward compatibility with Firebird while adding powerful new features for modern applications.</p>
            </div>
        </section>
        
        <section class="sb-tool-mapping">
            <h2>Tool Name Changes</h2>
            <table class="sb-migration-table">
                <thead>
                    <tr><th>Firebird Tool</th><th>ScratchBird Tool</th><th>Functionality</th></tr>
                </thead>
                <tbody>
                    <tr><td><code>gbak</code></td><td><code>sb_gbak</code></td><td>Backup and restore</td></tr>
                    <tr><td><code>gstat</code></td><td><code>sb_gstat</code></td><td>Database statistics</td></tr>
                    <tr><td><code>gfix</code></td><td><code>sb_gfix</code></td><td>Database maintenance</td></tr>
                    <tr><td><code>gsec</code></td><td><code>sb_gsec</code></td><td>Security management</td></tr>
                    <tr><td><code>isql</code></td><td><code>sb_isql</code></td><td>Interactive SQL</td></tr>
                    <tr><td><code>gpre</code></td><td><code>sb_gpre</code></td><td>SQL preprocessor</td></tr>
                    <tr><td><code>fbsvcmgr</code></td><td><code>sb_svcmgr</code></td><td>Service management</td></tr>
                </tbody>
            </table>
        </section>
        
        <section class="sb-configuration-changes">
            <h2>Configuration Changes</h2>
            <ul>
                <li><strong>Configuration File</strong>: <code>firebird.conf</code> → <code>scratchbird.conf</code></li>
                <li><strong>Client Library</strong>: <code>libfbclient</code> → <code>libsbclient</code></li>
                <li><strong>Database Extension</strong>: <code>.fdb</code> → <code>.sdb</code> (recommended)</li>
                <li><strong>Default Port</strong>: 3050 (unchanged for compatibility)</li>
            </ul>
        </section>
        
        <section class="sb-new-features">
            <h2>New ScratchBird 6.0 Features</h2>
            
            <div class="sb-feature-highlight">
                <h3>Hierarchical Schemas</h3>
                <p>Organize your database with nested schemas up to 8 levels deep:</p>
                <pre><code>CREATE SCHEMA company.division.department;
CREATE TABLE company.division.department.employees (...);</code></pre>
            </div>
            
            <div class="sb-feature-highlight">
                <h3>Vector Datatype for AI/ML</h3>
                <p>Store and query vector embeddings for machine learning applications:</p>
                <pre><code>CREATE TABLE documents (
    id INTEGER,
    embedding VECTOR(1536),
    content TEXT
);

SELECT * FROM documents 
ORDER BY embedding <-> '[0.1, 0.2, 0.3, ...]'::VECTOR
LIMIT 5;</code></pre>
            </div>
            
            <div class="sb-feature-highlight">
                <h3>Database Links</h3>
                <p>Access remote databases with schema-aware connections:</p>
                <pre><code>CREATE DATABASE LINK remote_hr 
TO 'remote_server:hr_database'
SCHEMA_MODE HIERARCHICAL
LOCAL_SCHEMA 'hr'
REMOTE_SCHEMA 'human_resources';

SELECT * FROM employees@remote_hr;</code></pre>
            </div>
            
            <div class="sb-feature-highlight">
                <h3>Enhanced Array Operations</h3>
                <p>PostgreSQL-compatible array operators and functions:</p>
                <pre><code>SELECT * FROM products 
WHERE tags @> ARRAY['electronics', 'mobile'];

SELECT ARRAY_LENGTH(categories, 1) FROM products;</code></pre>
            </div>
        </section>
        
        <section class="sb-migration-steps">
            <h2>Step-by-Step Migration</h2>
            <ol>
                <li><strong>Install ScratchBird</strong> alongside your existing Firebird installation</li>
                <li><strong>Update Scripts</strong>: Replace tool names (gbak → sb_gbak, etc.)</li>
                <li><strong>Test Applications</strong>: Verify compatibility with existing code</li>
                <li><strong>Migrate Configuration</strong>: Copy firebird.conf → scratchbird.conf</li>
                <li><strong>Update Client Libraries</strong>: Link against libsbclient</li>
                <li><strong>Leverage New Features</strong>: Implement hierarchical schemas and vector search</li>
                <li><strong>Performance Optimization</strong>: Use new indexing and query features</li>
            </ol>
        </section>
        
        <section class="sb-compatibility-notes">
            <h2>Compatibility Notes</h2>
            <div class="sb-compatibility-matrix">
                <h3>✅ Fully Compatible</h3>
                <ul>
                    <li>All SQL DDL and DML statements</li>
                    <li>Stored procedures and functions</li>
                    <li>Triggers and constraints</li>
                    <li>User-defined functions (UDFs)</li>
                    <li>Backup and restore operations</li>
                </ul>
                
                <h3>🔄 Enhanced Features</h3>
                <ul>
                    <li>Schema management (now hierarchical)</li>
                    <li>Array operations (PostgreSQL-compatible)</li>
                    <li>Full-text search (expanded functionality)</li>
                    <li>Database connections (with database links)</li>
                </ul>
                
                <h3>⭐ New Capabilities</h3>
                <ul>
                    <li>Vector datatype for AI/ML applications</li>
                    <li>Point geometric type for spatial data</li>
                    <li>Schema-aware database links</li>
                    <li>Enhanced network data types</li>
                </ul>
            </div>
        </section>
    </main>
    
    <script src="../assets/scratchbird-docs.js"></script>
</body>
</html>'''

        migration_file = self.target_doc_dir / '12-migration.html'
        with open(migration_file, 'w', encoding='utf-8') as f:
            f.write(migration_content)
            
        print(f"✅ Created migration guide: {migration_file}")
        return migration_file
        
    def insert_content_into_html(self, target_file, content, section_title):
        """Insert content into HTML file (utility method)"""
        try:
            if not target_file.exists():
                return False
                
            with open(target_file, 'r', encoding='utf-8') as f:
                html_content = f.read()
                
            from bs4 import BeautifulSoup
            soup = BeautifulSoup(html_content, 'html.parser')
            
            # Find insertion point
            main_tag = soup.find('main') or soup.find('body')
            if not main_tag:
                return False
                
            # Create new section
            new_section = soup.new_tag('section', **{'class': 'sb-migrated-content'})
            header = soup.new_tag('h2', **{'class': 'sb-migrated-title'})
            header.string = section_title
            new_section.append(header)
            
            content_div = soup.new_tag('div', **{'class': 'sb-migrated-body'})
            from bs4 import BeautifulSoup as ContentSoup
            content_soup = ContentSoup(content, 'html.parser')
            content_div.append(content_soup)
            new_section.append(content_div)
            
            # Insert before scripts
            script_tag = soup.find('script')
            if script_tag:
                script_tag.insert_before(new_section)
            else:
                main_tag.append(new_section)
                
            # Write back
            with open(target_file, 'w', encoding='utf-8') as f:
                f.write(str(soup))
                
            return True
            
        except Exception as e:
            print(f"❌ Error inserting content: {e}")
            return False
            
    def update_master_navigation(self):
        """Update master index to include new sections"""
        try:
            index_file = self.target_doc_dir / 'index.html'
            
            with open(index_file, 'r', encoding='utf-8') as f:
                content = f.read()
                
            # Add new navigation items
            new_nav_items = '''
            <li><a href="11-utilities.html">ScratchBird Utilities</a></li>
            <li><a href="12-migration.html">Migration Guide</a></li>'''
            
            # Insert before closing </ul>
            content = content.replace('</ul>', new_nav_items + '\n        </ul>')
            
            with open(index_file, 'w', encoding='utf-8') as f:
                f.write(content)
                
            print("✅ Updated master navigation")
            
        except Exception as e:
            print(f"❌ Error updating navigation: {e}")
            
    def run(self):
        """Execute the complete migration process"""
        print("🚀 Starting remaining documentation migration...")
        
        # Process high-priority files
        high_priority_count = self.integrate_high_priority_files()
        
        # Process SQL extensions
        sql_ext_count = self.process_sql_extensions()
        
        # Create utilities section
        utility_count = self.create_utilities_section()
        
        # Create migration guide
        migration_guide = self.create_migration_guide()
        
        # Update navigation
        self.update_master_navigation()
        
        total_processed = high_priority_count + sql_ext_count + utility_count + (1 if migration_guide else 0)
        
        print(f"\n🎉 Migration Complete!")
        print(f"   📊 Total files processed: {total_processed}")
        print(f"   📁 Documentation location: {self.target_doc_dir}")
        print(f"   🌐 Access via: {self.target_doc_dir}/index.html")
        
        return True


def main():
    source_doc_dir = "/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/doc"
    target_doc_dir = "/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/doc/v0.6.0/split_documentation"
    
    migrator = DocumentationMigrator(source_doc_dir, target_doc_dir)
    success = migrator.run()
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())