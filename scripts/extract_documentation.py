#!/usr/bin/env python3
"""
ScratchBird Documentation Extraction Script
Converts the large HTML Firebird documentation into structured ScratchBird documentation

Usage: python3 extract_documentation.py
"""

import os
import re
import sys
from pathlib import Path
from bs4 import BeautifulSoup
import json

class DocumentationExtractor:
    def __init__(self, source_html, output_dir):
        self.source_html = Path(source_html)
        self.output_dir = Path(output_dir)
        self.soup = None
        
        # Rebranding mappings
        self.firebird_replacements = {
            r'\bFirebird\b': 'ScratchBird',
            r'\bfirebird\b': 'scratchbird',
            r'Firebird 5\.0': 'ScratchBird 6.0',
            r'firebird-5\.0': 'scratchbird-6.0',
            r'fblangref50': 'sblangref60',
            r'firebirdsql\.org': 'scratchbird.org',
            r'Firebird Project': 'ScratchBird Project',
            r'Firebird SQL': 'ScratchBird SQL',
            r'Firebird server': 'ScratchBird server',
            r'Firebird engine': 'ScratchBird engine',
            r'Firebird database': 'ScratchBird database',
        }
        
        self.utility_replacements = {
            r'\bgbak\b(?![\w_])': 'sb_gbak',
            r'\bgstat\b(?![\w_])': 'sb_gstat', 
            r'\bgsec\b(?![\w_])': 'sb_gsec',
            r'\bgfix\b(?![\w_])': 'sb_gfix',
            r'\bisql\b(?![\w_])': 'sb_isql',
            r'\bgpre\b(?![\w_])': 'sb_gpre',
            r'\bfbsvcmgr\b(?![\w_])': 'sb_svcmgr',
            r'\bnbackup\b(?![\w_])': 'sb_nbackup',
            r'\bfbtrace\b(?![\w_])': 'sb_trace',
        }
        
        # Document sections for splitting
        self.doc_sections = [
            {
                'name': '01-introduction',
                'title': 'Introduction and Language Structure', 
                'sections': ['about-the-firebird-50-language-reference', 'language-structure']
            },
            {
                'name': '02-datatypes',
                'title': 'Data Types and Subtypes',
                'sections': ['datatypes-subtypes']
            },
            {
                'name': '03-language-elements', 
                'title': 'Common Language Elements',
                'sections': ['common-language-elements']
            },
            {
                'name': '04-data-manipulation',
                'title': 'Data Manipulation Language',
                'sections': ['dml']
            },
            {
                'name': '05-transactions-psql',
                'title': 'Transaction Control and PSQL',
                'sections': ['transaction-control', 'psql']
            },
            {
                'name': '06-data-definition',
                'title': 'Data Definition Language', 
                'sections': ['ddl']
            },
            {
                'name': '07-security-functions',
                'title': 'Security and Built-in Functions',
                'sections': ['security', 'builtin-functions']
            },
            {
                'name': '08-system-features',
                'title': 'System Features and Monitoring',
                'sections': ['builtin-packages', 'monitoring']
            },
            {
                'name': '09-administration',
                'title': 'Database Administration',
                'sections': ['management-statements']
            },
            {
                'name': '10-reference',
                'title': 'Reference Material',
                'sections': ['appx-errorcodes', 'appx-reserved-words', 'appx-charsets', 'appx-systables']
            }
        ]
        
    def load_html(self):
        """Load and parse the HTML documentation"""
        print(f"Loading HTML documentation from {self.source_html}")
        try:
            with open(self.source_html, 'r', encoding='utf-8') as f:
                content = f.read()
            self.soup = BeautifulSoup(content, 'html.parser')
            print(f"Successfully loaded {len(content)} characters")
            return True
        except Exception as e:
            print(f"Error loading HTML: {e}")
            return False
            
    def apply_rebranding(self, text):
        """Apply systematic rebranding to text content"""
        if not isinstance(text, str):
            return text
            
        # Apply Firebird -> ScratchBird replacements
        for pattern, replacement in self.firebird_replacements.items():
            text = re.sub(pattern, replacement, text)
            
        # Apply utility tool rebranding  
        for pattern, replacement in self.utility_replacements.items():
            text = re.sub(pattern, replacement, text)
            
        return text
        
    def extract_css_assets(self):
        """Extract CSS and create rebranded stylesheet"""
        print("Extracting CSS assets...")
        
        # Create assets directory
        assets_dir = self.output_dir / 'assets'
        assets_dir.mkdir(parents=True, exist_ok=True)
        
        # Extract embedded CSS
        css_content = ""
        style_tags = self.soup.find_all('style')
        
        for style in style_tags:
            css_content += style.get_text()
            
        # Rebrand CSS content
        css_content = self.apply_rebranding(css_content)
        
        # Update CSS color scheme for ScratchBird branding
        css_content = self.update_css_branding(css_content)
        
        # Save rebranded CSS
        css_file = assets_dir / 'scratchbird-docs.css'
        with open(css_file, 'w', encoding='utf-8') as f:
            f.write(css_content)
            
        print(f"Saved CSS to {css_file}")
        return css_file
        
    def update_css_branding(self, css_content):
        """Update CSS colors and branding for ScratchBird"""
        
        # ScratchBird color scheme (earth tones, professional)
        color_updates = {
            r'#2156a5': '#8B4513',  # Primary blue -> Saddle brown
            r'#1d4b8f': '#A0522D',  # Dark blue -> Sienna  
            r'#dddddf': '#DEB887',  # Light gray -> Burlywood
            r'#dedede': '#D2B48C',  # Border gray -> Tan
            r'rgba\(0,0,0,\.8\)': 'rgba(101,67,33,0.8)',  # Dark text -> Dark brown
            r'rgba\(0,0,0,\.85\)': 'rgba(101,67,33,0.85)', # Darker text -> Darker brown
        }
        
        for old_color, new_color in color_updates.items():
            css_content = re.sub(old_color, new_color, css_content)
            
        # Add ScratchBird-specific CSS
        scratchbird_css = '''
        
        /* ScratchBird-specific styling */
        .scratchbird-brand {
            color: #8B4513;
            font-weight: bold;
        }
        
        .sb-version {
            background: linear-gradient(135deg, #8B4513, #A0522D);
            color: white;
            padding: 2px 6px;
            border-radius: 3px;
            font-size: 0.8em;
        }
        
        .sb-new-feature {
            background: #DEB887;
            border-left: 4px solid #8B4513;
            padding: 8px 12px;
            margin: 10px 0;
        }
        
        .sb-utility-ref {
            font-family: monospace;
            background: #F5F5DC;
            padding: 1px 3px;
            border: 1px solid #D2B48C;
            border-radius: 2px;
        }
        '''
        
        css_content += scratchbird_css
        return css_content
        
    def extract_javascript(self):
        """Extract JavaScript functionality"""
        print("Extracting JavaScript assets...")
        
        assets_dir = self.output_dir / 'assets' 
        
        # Extract embedded JavaScript
        js_content = ""
        script_tags = self.soup.find_all('script')
        
        for script in script_tags:
            if script.get_text().strip():
                js_content += script.get_text() + "\n\n"
                
        # Rebrand JavaScript comments and references
        js_content = self.apply_rebranding(js_content)
        
        # Save JavaScript
        js_file = assets_dir / 'scratchbird-docs.js'
        with open(js_file, 'w', encoding='utf-8') as f:
            f.write(js_content)
            
        print(f"Saved JavaScript to {js_file}")
        return js_file
        
    def create_section_html(self, section_config, section_content):
        """Create HTML file for a documentation section"""
        
        # Create base HTML template
        html_template = f'''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{section_config['title']} - ScratchBird 6.0 Language Reference</title>
    <link rel="stylesheet" href="../assets/scratchbird-docs.css">
</head>
<body>
    <nav class="sb-nav">
        <a href="../index.html">← Back to Documentation Index</a>
        <div class="sb-version">ScratchBird v6.0</div>
    </nav>
    
    <main class="sb-content">
        <h1>{section_config['title']}</h1>
        {section_content}
    </main>
    
    <script src="../assets/scratchbird-docs.js"></script>
</body>
</html>'''

        return html_template
        
    def create_master_index(self):
        """Create master documentation index"""
        print("Creating master documentation index...")
        
        index_html = '''<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ScratchBird 6.0 Language Reference</title>
    <link rel="stylesheet" href="assets/scratchbird-docs.css">
</head>
<body>
    <header class="sb-header">
        <h1><span class="scratchbird-brand">ScratchBird</span> 6.0 Language Reference</h1>
        <p>Complete language reference for ScratchBird database system</p>
        <div class="sb-version">v6.0.0</div>
    </header>
    
    <nav class="sb-main-nav">
        <h2>Documentation Sections</h2>
        <ul>'''
        
        for section in self.doc_sections:
            index_html += f'''
            <li><a href="{section['name']}/index.html">{section['title']}</a></li>'''
            
        index_html += '''
        </ul>
    </nav>
    
    <section class="sb-new-features">
        <h2>New in ScratchBird 6.0</h2>
        <div class="sb-new-feature">
            <h3>Vector Datatype for AI/ML</h3>
            <p>Multi-dimensional vector support with similarity operators for machine learning applications.</p>
        </div>
        <div class="sb-new-feature">
            <h3>Hierarchical Schemas</h3>
            <p>8-level nested schema support for enterprise data organization.</p>  
        </div>
        <div class="sb-new-feature">
            <h3>Enhanced Array Operations</h3>
            <p>PostgreSQL-compatible array operators and functions.</p>
        </div>
        <div class="sb-new-feature">
            <h3>Geometric Types</h3>
            <p>2D point support for spatial data applications.</p>
        </div>
    </section>
    
    <script src="assets/scratchbird-docs.js"></script>
</body>
</html>'''

        index_file = self.output_dir / 'index.html'
        with open(index_file, 'w', encoding='utf-8') as f:
            f.write(index_html)
            
        print(f"Created master index at {index_file}")
        
    def run(self):
        """Execute the full extraction process"""
        print("Starting ScratchBird Documentation Extraction...")
        
        if not self.load_html():
            return False
            
        # Create output directory structure
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Extract assets
        self.extract_css_assets()
        self.extract_javascript()
        
        # Create master index
        self.create_master_index()
        
        print("Documentation extraction completed successfully!")
        return True


def main():
    # Configuration
    source_html = "/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/doc/v0.6.0/Firebird 5.0 Language Reference.html"
    output_dir = "/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/docs/v0.6.0"
    
    if not os.path.exists(source_html):
        print(f"Error: Source HTML file not found: {source_html}")
        return 1
        
    extractor = DocumentationExtractor(source_html, output_dir)
    success = extractor.run()
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())