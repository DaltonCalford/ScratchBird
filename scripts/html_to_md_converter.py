#!/usr/bin/env python3
"""
HTML to Markdown Converter for ScratchBird Documentation
Converts HTML documentation files to clean Markdown format.
"""

import re
import sys
from pathlib import Path

def clean_text(text):
    """Clean and normalize text content."""
    if not text:
        return ""
    
    # Remove excessive whitespace
    text = re.sub(r'\s+', ' ', text)
    # Trim leading/trailing whitespace
    text = text.strip()
    # Convert HTML entities
    text = text.replace('&lt;', '<')
    text = text.replace('&gt;', '>')
    text = text.replace('&amp;', '&')
    text = text.replace('&quot;', '"')
    text = text.replace('&apos;', "'")
    text = text.replace('&nbsp;', ' ')
    
    return text

def extract_table_content(html_content):
    """Extract and convert HTML tables to Markdown tables."""
    tables = []
    
    # Find all table elements
    table_pattern = r'<table[^>]*?>(.*?)</table>'
    table_matches = re.findall(table_pattern, html_content, re.DOTALL | re.IGNORECASE)
    
    for table_html in table_matches:
        # Extract table caption if present
        caption_match = re.search(r'<caption[^>]*?>(.*?)</caption>', table_html, re.DOTALL | re.IGNORECASE)
        caption = ""
        if caption_match:
            caption = clean_text(re.sub(r'<[^>]*>', '', caption_match.group(1)))
        
        # Extract table rows
        tbody_match = re.search(r'<tbody[^>]*?>(.*?)</tbody>', table_html, re.DOTALL | re.IGNORECASE)
        thead_match = re.search(r'<thead[^>]*?>(.*?)</thead>', table_html, re.DOTALL | re.IGNORECASE)
        
        rows = []
        
        # Process header rows
        if thead_match:
            header_rows = re.findall(r'<tr[^>]*?>(.*?)</tr>', thead_match.group(1), re.DOTALL | re.IGNORECASE)
            for row_html in header_rows:
                cells = re.findall(r'<t[hd][^>]*?>(.*?)</t[hd]>', row_html, re.DOTALL | re.IGNORECASE)
                clean_cells = [clean_text(re.sub(r'<[^>]*>', '', cell)) for cell in cells]
                if clean_cells:
                    rows.append(clean_cells)
        
        # Process body rows
        if tbody_match:
            body_rows = re.findall(r'<tr[^>]*?>(.*?)</tr>', tbody_match.group(1), re.DOTALL | re.IGNORECASE)
            for row_html in body_rows:
                cells = re.findall(r'<t[hd][^>]*?>(.*?)</t[hd]>', row_html, re.DOTALL | re.IGNORECASE)
                clean_cells = [clean_text(re.sub(r'<[^>]*>', '', cell)) for cell in cells]
                if clean_cells:
                    rows.append(clean_cells)
        
        # Convert to Markdown table
        if rows:
            markdown_table = []
            
            if caption:
                markdown_table.append(f"**{caption}**\n")
            
            # Add header row
            if rows:
                header = rows[0]
                markdown_table.append("| " + " | ".join(header) + " |")
                markdown_table.append("| " + " | ".join(["---"] * len(header)) + " |")
                
                # Add data rows
                for row in rows[1:]:
                    # Pad row to match header length
                    while len(row) < len(header):
                        row.append("")
                    markdown_table.append("| " + " | ".join(row[:len(header)]) + " |")
            
            tables.append("\n".join(markdown_table))
    
    return tables

def convert_html_to_markdown(html_content):
    """Convert HTML content to Markdown format."""
    markdown_lines = []
    
    # Remove CSS styles and scripts
    html_content = re.sub(r'<style[^>]*>.*?</style>', '', html_content, flags=re.DOTALL | re.IGNORECASE)
    html_content = re.sub(r'<script[^>]*>.*?</script>', '', html_content, flags=re.DOTALL | re.IGNORECASE)
    
    # Extract main content area
    content_match = re.search(r'<div id="content"[^>]*>(.*?)</div>\s*</body>', html_content, re.DOTALL | re.IGNORECASE)
    if content_match:
        html_content = content_match.group(1)
    
    # Split into lines for processing
    lines = html_content.split('\n')
    i = 0
    
    while i < len(lines):
        line = lines[i].strip()
        
        # Skip empty lines initially
        if not line:
            i += 1
            continue
            
        # Convert headings
        heading_match = re.match(r'<h([1-6])[^>]*>.*?<a[^>]*></a>(.*?)</h\1>', line, re.IGNORECASE)
        if not heading_match:
            heading_match = re.match(r'<h([1-6])[^>]*>(.*?)</h\1>', line, re.IGNORECASE)
        
        if heading_match:
            level = int(heading_match.group(1))
            title_content = heading_match.group(2) if len(heading_match.groups()) > 1 else heading_match.group(1)
            title = clean_text(re.sub(r'<[^>]*>', '', title_content))
            markdown_lines.append("\n" + "#" * level + " " + title + "\n")
            i += 1
            continue
        
        # Convert paragraphs
        if line.startswith('<div class="paragraph">') or line.startswith('<p'):
            para_content = []
            # Collect paragraph content
            while i < len(lines) and not (lines[i].strip().endswith('</div>') and 'paragraph' in lines[i]) and not lines[i].strip().endswith('</p>'):
                para_content.append(lines[i])
                i += 1
            if i < len(lines):
                para_content.append(lines[i])
                i += 1
            
            # Clean paragraph content
            para_text = ' '.join(para_content)
            para_text = re.sub(r'<div[^>]*>', '', para_text)
            para_text = re.sub(r'</div>', '', para_text)
            para_text = re.sub(r'<p[^>]*>', '', para_text)
            para_text = re.sub(r'</p>', '', para_text)
            
            # Convert inline code
            para_text = re.sub(r'<code[^>]*>(.*?)</code>', r'`\1`', para_text)
            # Convert em/strong
            para_text = re.sub(r'<em[^>]*>(.*?)</em>', r'*\1*', para_text)
            para_text = re.sub(r'<strong[^>]*>(.*?)</strong>', r'**\1**', para_text)
            # Convert sup/sub
            para_text = re.sub(r'<sup[^>]*>(.*?)</sup>', r'^\1^', para_text)
            para_text = re.sub(r'<sub[^>]*>(.*?)</sub>', r'_\1_', para_text)
            
            # Convert links
            para_text = re.sub(r'<a[^>]*href="([^"]*)"[^>]*>(.*?)</a>', r'[\2](\1)', para_text)
            
            # Remove remaining HTML tags
            para_text = re.sub(r'<[^>]*>', '', para_text)
            para_text = clean_text(para_text)
            
            if para_text:
                markdown_lines.append(para_text + "\n")
            continue
        
        # Convert code blocks
        if '<div class="listingblock">' in line or '<pre class="highlight">' in line:
            code_content = []
            title = ""
            
            # Look for title
            j = i
            while j < len(lines) and '</div>' not in lines[j]:
                if '<div class="title">' in lines[j]:
                    title_line = lines[j]
                    title = clean_text(re.sub(r'<[^>]*>', '', title_line))
                elif '<pre' in lines[j]:
                    # Start collecting code
                    code_start = j
                    code_lines = []
                    while j < len(lines) and '</pre>' not in lines[j]:
                        if j > code_start:  # Skip the <pre> line itself
                            code_line = lines[j]
                            code_line = re.sub(r'<[^>]*>', '', code_line)  # Remove HTML tags
                            code_lines.append(code_line)
                        j += 1
                    if j < len(lines):
                        # Add the closing </pre> line content if any
                        close_line = lines[j]
                        close_content = re.sub(r'</pre>.*', '', close_line)
                        close_content = re.sub(r'<[^>]*>', '', close_content)
                        if close_content.strip():
                            code_lines.append(close_content)
                    break
                j += 1
            
            if code_lines:
                if title:
                    markdown_lines.append(f"**{title}**\n")
                markdown_lines.append("```")
                markdown_lines.extend(code_lines)
                markdown_lines.append("```\n")
            
            # Skip to after this block
            while i < len(lines) and '</div>' not in lines[i]:
                i += 1
            i += 1
            continue
        
        # Convert lists
        if '<div class="ulist">' in line or '<ul' in line:
            list_items = []
            j = i
            while j < len(lines) and ('</ul>' not in lines[j] and '</div>' not in lines[j]):
                if '<li>' in lines[j] or '<p>' in lines[j]:
                    item_content = []
                    while j < len(lines) and '</li>' not in lines[j]:
                        item_content.append(lines[j])
                        j += 1
                    if j < len(lines):
                        item_content.append(lines[j])
                    
                    item_text = ' '.join(item_content)
                    item_text = re.sub(r'<li[^>]*>', '', item_text)
                    item_text = re.sub(r'</li>', '', item_text)
                    item_text = re.sub(r'<p[^>]*>', '', item_text)
                    item_text = re.sub(r'</p>', '', item_text)
                    item_text = re.sub(r'<code[^>]*>(.*?)</code>', r'`\1`', item_text)
                    item_text = re.sub(r'<a[^>]*href="([^"]*)"[^>]*>(.*?)</a>', r'[\2](\1)', item_text)
                    item_text = re.sub(r'<em[^>]*>(.*?)</em>', r'*\1*', item_text)
                    item_text = re.sub(r'<strong[^>]*>(.*?)</strong>', r'**\1**', item_text)
                    item_text = re.sub(r'<[^>]*>', '', item_text)
                    item_text = clean_text(item_text)
                    
                    if item_text:
                        list_items.append(f"- {item_text}")
                j += 1
            
            if list_items:
                markdown_lines.extend(list_items)
                markdown_lines.append("")
            
            i = j + 1
            continue
        
        # Handle tables
        if '<table' in line:
            table_content = []
            j = i
            while j < len(lines) and '</table>' not in lines[j]:
                table_content.append(lines[j])
                j += 1
            if j < len(lines):
                table_content.append(lines[j])
            
            table_html = '\n'.join(table_content)
            tables = extract_table_content(table_html)
            if tables:
                markdown_lines.extend(tables)
                markdown_lines.append("")
            
            i = j + 1
            continue
        
        # Skip other HTML elements
        i += 1
    
    return '\n'.join(markdown_lines)

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 html_to_md_converter.py <input.html> <output.md>")
        sys.exit(1)
    
    input_file = Path(sys.argv[1])
    output_file = Path(sys.argv[2])
    
    if not input_file.exists():
        print(f"Error: Input file {input_file} does not exist")
        sys.exit(1)
    
    try:
        # Read HTML content
        with open(input_file, 'r', encoding='utf-8') as f:
            html_content = f.read()
        
        # Convert to Markdown
        markdown_content = convert_html_to_markdown(html_content)
        
        # Add header
        final_content = """# Data Types and Subtypes

*Comprehensive guide to ScratchBird data types including network address types, range types, JSON, enhanced UUID features, and advanced text types*

""" + markdown_content
        
        # Write Markdown content
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(final_content)
        
        print(f"Successfully converted {input_file} to {output_file}")
        
    except Exception as e:
        print(f"Error converting file: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()