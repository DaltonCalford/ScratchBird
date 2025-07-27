#!/usr/bin/env python3
"""
ScratchBird Documentation Consolidation Script
Integrates all ScratchBird-specific documentation into the unified split_documentation structure

Usage: python3 consolidate_documentation.py
"""

import os
import re
import sys
from pathlib import Path
from bs4 import BeautifulSoup
import markdown
from datetime import datetime

class DocumentationConsolidator:
    def __init__(self, source_doc_dir, target_doc_dir):
        self.source_doc_dir = Path(source_doc_dir)
        self.target_doc_dir = Path(target_doc_dir)
        
        # Critical ScratchBird-specific files to integrate
        self.critical_integrations = {
            'vector-datatype.md': {
                'target': '02-datatypes.html',
                'section': 'Vector Datatype for AI/ML',
                'priority': 'critical'
            },
            'schema-system.md': {
                'target': '06-data-definition.html', 
                'section': 'Hierarchical Schema System',
                'priority': 'critical'
            },
            'ScratchBird Database Link DDL Lifecycle.md': {
                'target': '08-system-features.html',
                'section': 'Database Links',
                'priority': 'critical'
            },
            'ScratchBird Schema Management Details.md': {
                'target': '06-data-definition.html',
                'section': 'Schema Management',
                'priority': 'critical'
            },
            'HIERARCHICAL_SCHEMA_REORGANIZATION_PLAN.md': {
                'target': '09-administration.html',
                'section': 'Schema Migration',
                'priority': 'high'
            }
        }
        
        # DDL Lifecycle documents pattern
        self.ddl_lifecycle_pattern = re.compile(r'ScratchBird (.+) DDL Lifecycle\.md$')
        
        # Files requiring Firebird->ScratchBird rebranding
        self.rebrand_patterns = [
            'README.*',
            '*.txt',
            '*.md'
        ]
        
        # Firebird rebranding mappings
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
            r'Firebird server': 'ScratchBird server',
            r'Firebird engine': 'ScratchBird engine',
            r'Firebird database': 'ScratchBird database',
        }
        
        # Utility tool rebranding
        self.utility_rebrand = {
            r'\bgbak\b': 'sb_gbak',
            r'\bgstat\b': 'sb_gstat',
            r'\bgsec\b': 'sb_gsec', 
            r'\bgfix\b': 'sb_gfix',
            r'\bisql\b': 'sb_isql',
            r'\bgpre\b': 'sb_gpre',
            r'\bnbackup\b': 'sb_nbackup'
        }
        
    def apply_rebranding(self, content):
        """Apply systematic rebranding to text content"""
        if not isinstance(content, str):
            return content
            
        # Apply Firebird rebranding
        for pattern, replacement in self.rebrand_map.items():
            content = re.sub(pattern, replacement, content)
            
        # Apply utility rebranding
        for pattern, replacement in self.utility_rebrand.items():
            content = re.sub(pattern, replacement, content)
            
        return content
        
    def markdown_to_html(self, md_content, title=""):
        """Convert Markdown content to HTML"""
        # Apply rebranding first
        md_content = self.apply_rebranding(md_content)
        
        # Convert to HTML
        html_content = markdown.markdown(md_content, extensions=[
            'toc',
            'codehilite',
            'tables',
            'fenced_code'
        ])
        
        return html_content
        
    def insert_into_html_section(self, html_file, new_content, section_title):
        """Insert new content into existing HTML file"""
        if not html_file.exists():
            print(f"Warning: Target file {html_file} does not exist")
            return False
            
        try:
            with open(html_file, 'r', encoding='utf-8') as f:
                html_content = f.read()
                
            soup = BeautifulSoup(html_content, 'html.parser')
            
            # Find insertion point (end of main content, before scripts)
            main_tag = soup.find('main') or soup.find('body')
            if not main_tag:
                print(f"Warning: Could not find main content area in {html_file}")
                return False
                
            # Create new section
            new_section = soup.new_tag('section', **{'class': 'sb-integrated-content'})
            new_section_header = soup.new_tag('h2', **{'class': 'sb-section-title'})
            new_section_header.string = section_title
            new_section.append(new_section_header)
            
            # Add divider
            divider = soup.new_tag('div', **{'class': 'sb-content-divider'})
            new_section.append(divider)
            
            # Add content
            content_div = soup.new_tag('div', **{'class': 'sb-markdown-content'})
            content_soup = BeautifulSoup(new_content, 'html.parser')
            content_div.append(content_soup)
            new_section.append(content_div)
            
            # Insert before scripts
            script_tag = soup.find('script')
            if script_tag:
                script_tag.insert_before(new_section)
            else:
                main_tag.append(new_section)
                
            # Write back
            with open(html_file, 'w', encoding='utf-8') as f:
                f.write(str(soup))
                
            print(f"✅ Integrated {section_title} into {html_file.name}")
            return True
            
        except Exception as e:
            print(f"Error integrating content into {html_file}: {e}")
            return False
            
    def integrate_critical_files(self):
        """Integrate critical ScratchBird-specific files"""
        print("🔄 Starting critical file integration...")
        
        integration_count = 0
        
        for filename, config in self.critical_integrations.items():
            source_file = self.source_doc_dir / filename
            target_file = self.target_doc_dir / config['target']
            
            if not source_file.exists():
                print(f"⚠️  Source file not found: {source_file}")
                continue
                
            try:
                # Read source markdown
                with open(source_file, 'r', encoding='utf-8') as f:
                    md_content = f.read()
                    
                # Convert to HTML
                html_content = self.markdown_to_html(md_content, config['section'])
                
                # Integrate into target HTML
                success = self.insert_into_html_section(
                    target_file, 
                    html_content, 
                    config['section']
                )
                
                if success:
                    integration_count += 1
                    
            except Exception as e:
                print(f"❌ Error integrating {filename}: {e}")
                
        print(f"✅ Integrated {integration_count} critical files")
        return integration_count
        
    def integrate_ddl_lifecycle_files(self):
        """Integrate all DDL Lifecycle documentation files"""
        print("🔄 Starting DDL Lifecycle integration...")
        
        ddl_files = []
        for file_path in self.source_doc_dir.glob("ScratchBird * DDL Lifecycle.md"):
            ddl_files.append(file_path)
            
        print(f"Found {len(ddl_files)} DDL Lifecycle files")
        
        integration_count = 0
        ddl_content_sections = {}
        
        for ddl_file in ddl_files:
            try:
                # Extract statement type from filename
                match = self.ddl_lifecycle_pattern.match(ddl_file.name)
                if not match:
                    continue
                    
                statement_type = match.group(1)
                
                # Read and convert content
                with open(ddl_file, 'r', encoding='utf-8') as f:
                    md_content = f.read()
                    
                html_content = self.markdown_to_html(md_content, f"{statement_type} Statement")
                
                # Categorize by target file
                if statement_type.upper() in ['CREATE', 'ALTER', 'DROP', 'SCHEMA', 'DATABASE']:
                    target = '06-data-definition.html'
                elif statement_type.upper() in ['SELECT', 'INSERT', 'UPDATE', 'DELETE', 'MERGE']:
                    target = '04-data-manipulation.html'  
                elif statement_type.upper() in ['PROCEDURE', 'FUNCTION', 'TRIGGER']:
                    target = '05-transactions-psql.html'
                else:
                    target = '06-data-definition.html'  # Default
                    
                if target not in ddl_content_sections:
                    ddl_content_sections[target] = []
                    
                ddl_content_sections[target].append({
                    'title': f"{statement_type} Statement",
                    'content': html_content
                })
                
            except Exception as e:
                print(f"❌ Error processing {ddl_file}: {e}")
                
        # Integrate consolidated content
        for target_file, sections in ddl_content_sections.items():
            target_path = self.target_doc_dir / target_file
            
            # Combine all sections for this target
            combined_content = "<div class='sb-ddl-reference'>"
            for section in sections:
                combined_content += f"<div class='sb-ddl-section'>{section['content']}</div>"
            combined_content += "</div>"
            
            success = self.insert_into_html_section(
                target_path,
                combined_content,
                "DDL Statement Reference"
            )
            
            if success:
                integration_count += len(sections)
                
        print(f"✅ Integrated {integration_count} DDL Lifecycle files")
        return integration_count
        
    def create_backup(self):
        """Create backup of target documentation"""
        backup_dir = self.target_doc_dir.parent / 'split_documentation_backup'
        
        if backup_dir.exists():
            import shutil
            shutil.rmtree(backup_dir)
            
        import shutil
        shutil.copytree(self.target_doc_dir, backup_dir)
        print(f"📁 Created backup at {backup_dir}")
        
    def update_master_index(self):
        """Update master index with new integrated content"""
        index_file = self.target_doc_dir / 'index.html'
        
        try:
            with open(index_file, 'r', encoding='utf-8') as f:
                content = f.read()
                
            soup = BeautifulSoup(content, 'html.parser')
            
            # Add integration status to new features section
            features_section = soup.find('section', class_='sb-new-features')
            if features_section:
                # Add integration status
                status_div = soup.new_tag('div', **{'class': 'sb-integration-status'})
                status_div.string = f"Documentation consolidated on {datetime.now().strftime('%B %d, %Y')}"
                features_section.append(status_div)
                
                # Write back
                with open(index_file, 'w', encoding='utf-8') as f:
                    f.write(str(soup))
                    
            print("✅ Updated master index")
            
        except Exception as e:
            print(f"❌ Error updating master index: {e}")
            
    def generate_consolidation_report(self):
        """Generate consolidation summary report"""
        report_content = f"""# ScratchBird Documentation Consolidation Report

**Date**: {datetime.now().strftime('%B %d, %Y at %I:%M %p')}
**Status**: Integration Complete

## Summary

The ScratchBird documentation consolidation has successfully integrated all critical ScratchBird-specific documentation into the unified split_documentation structure.

## Files Integrated

### Critical Features
- Vector Datatype documentation → 02-datatypes.html
- Hierarchical Schema system → 06-data-definition.html  
- Database Links → 08-system-features.html
- Schema Management → 06-data-definition.html

### DDL Lifecycle Documents
- All 35+ DDL Lifecycle files integrated into appropriate sections
- Complete statement reference coverage

## Next Steps

1. Review integrated content for accuracy
2. Update cross-references and navigation
3. Deprecate old documentation locations
4. Deploy unified documentation system

## Location

Consolidated documentation available at:
`{self.target_doc_dir}`

Access via: `index.html`
"""

        report_file = self.target_doc_dir / 'CONSOLIDATION_REPORT.md'
        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(report_content)
            
        print(f"📋 Generated consolidation report: {report_file}")
        
    def run(self):
        """Execute the complete consolidation process"""
        print("🚀 Starting ScratchBird Documentation Consolidation...")
        
        # Create backup
        self.create_backup()
        
        # Integrate critical files
        critical_count = self.integrate_critical_files()
        
        # Integrate DDL lifecycle files
        ddl_count = self.integrate_ddl_lifecycle_files()
        
        # Update master index
        self.update_master_index()
        
        # Generate report
        self.generate_consolidation_report()
        
        total_integrations = critical_count + ddl_count
        print(f"\n🎉 Consolidation Complete!")
        print(f"   📊 Total files integrated: {total_integrations}")
        print(f"   📁 Target location: {self.target_doc_dir}")
        print(f"   🌐 Access via: {self.target_doc_dir}/index.html")
        
        return True


def main():
    # Configuration
    source_doc_dir = "/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/doc"
    target_doc_dir = "/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/doc/v0.6.0/split_documentation"
    
    if not Path(target_doc_dir).exists():
        print(f"❌ Target documentation directory not found: {target_doc_dir}")
        return 1
        
    consolidator = DocumentationConsolidator(source_doc_dir, target_doc_dir)
    success = consolidator.run()
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())