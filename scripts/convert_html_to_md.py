#!/usr/bin/env python3
"""
HTML to Markdown converter for ScratchBird documentation
Converts HTML content to clean Markdown format
"""

import re
import sys
from pathlib import Path

def clean_html_content(html_content):
    """Extract and clean HTML content, converting to Markdown"""
    
    # Find the main content section
    content_match = re.search(r'<div id="content">(.*?)</div>\s*</div>\s*</body>', html_content, re.DOTALL)
    if not content_match:
        # Fallback: try to find content after the CSS
        content_match = re.search(r'</style>\s*</head>\s*<body[^>]*>(.*?)</body>', html_content, re.DOTALL)
    
    if not content_match:
        print("Could not find main content section")
        return ""
    
    content = content_match.group(1)
    
    # Remove navigation and header elements
    content = re.sub(r'<nav[^>]*>.*?</nav>', '', content, flags=re.DOTALL)
    content = re.sub(r'<header[^>]*>.*?</header>', '', content, flags=re.DOTALL)
    content = re.sub(r'<div class="navigation[^"]*"[^>]*>.*?</div>', '', content, flags=re.DOTALL)
    
    # Convert HTML headings to Markdown
    content = re.sub(r'<h1[^>]*>(.*?)</h1>', r'# \1', content, flags=re.DOTALL)
    content = re.sub(r'<h2[^>]*>(.*?)</h2>', r'## \1', content, flags=re.DOTALL)
    content = re.sub(r'<h3[^>]*>(.*?)</h3>', r'### \1', content, flags=re.DOTALL)
    content = re.sub(r'<h4[^>]*>(.*?)</h4>', r'#### \1', content, flags=re.DOTALL)
    content = re.sub(r'<h5[^>]*>(.*?)</h5>', r'##### \1', content, flags=re.DOTALL)
    content = re.sub(r'<h6[^>]*>(.*?)</h6>', r'###### \1', content, flags=re.DOTALL)
    
    # Convert code blocks
    content = re.sub(r'<pre class="highlight"><code[^>]*>(.*?)</code></pre>', r'```sql\n\1\n```', content, flags=re.DOTALL)
    content = re.sub(r'<pre[^>]*>(.*?)</pre>', r'```\n\1\n```', content, flags=re.DOTALL)
    content = re.sub(r'<div class="listingblock"[^>]*>.*?<div class="content"[^>]*>(.*?)</div>', r'```\n\1\n```', content, flags=re.DOTALL)
    
    # Convert inline code
    content = re.sub(r'<code[^>]*>(.*?)</code>', r'`\1`', content, flags=re.DOTALL)
    
    # Convert links
    content = re.sub(r'<a[^>]*href="([^"]*)"[^>]*>(.*?)</a>', r'[\2](\1)', content, flags=re.DOTALL)
    
    # Convert paragraphs
    content = re.sub(r'<div class="paragraph"[^>]*>\s*<p[^>]*>(.*?)</p>\s*</div>', r'\1\n', content, flags=re.DOTALL)
    content = re.sub(r'<p[^>]*>(.*?)</p>', r'\1\n', content, flags=re.DOTALL)
    
    # Convert emphasis and strong
    content = re.sub(r'<em[^>]*>(.*?)</em>', r'*\1*', content, flags=re.DOTALL)
    content = re.sub(r'<i[^>]*>(.*?)</i>', r'*\1*', content, flags=re.DOTALL)
    content = re.sub(r'<strong[^>]*>(.*?)</strong>', r'**\1**', content, flags=re.DOTALL)
    content = re.sub(r'<b[^>]*>(.*?)</b>', r'**\1**', content, flags=re.DOTALL)
    
    # Convert lists
    content = re.sub(r'<ul[^>]*>', '', content)
    content = re.sub(r'</ul>', '', content)
    content = re.sub(r'<ol[^>]*>', '', content)
    content = re.sub(r'</ol>', '', content)
    content = re.sub(r'<li[^>]*>(.*?)</li>', r'- \1', content, flags=re.DOTALL)
    
    # Convert tables
    content = convert_tables(content)
    
    # Convert definition lists
    content = re.sub(r'<dt[^>]*>(.*?)</dt>\s*<dd[^>]*>(.*?)</dd>', r'**\1**: \2\n', content, flags=re.DOTALL)
    
    # Remove remaining HTML tags but preserve content
    content = re.sub(r'<div[^>]*class="title"[^>]*>(.*?)</div>', r'**\1**\n', content, flags=re.DOTALL)
    content = re.sub(r'<[^>]+>', '', content)
    
    # Clean up whitespace
    content = re.sub(r'\n\s*\n\s*\n+', '\n\n', content)
    content = re.sub(r'^\s+', '', content, flags=re.MULTILINE)
    content = re.sub(r'\s+$', '', content, flags=re.MULTILINE)
    
    # Decode HTML entities
    content = content.replace('&lt;', '<')
    content = content.replace('&gt;', '>')
    content = content.replace('&amp;', '&')
    content = content.replace('&quot;', '"')
    content = content.replace('&#39;', "'")
    
    return content.strip()

def convert_tables(content):
    """Convert HTML tables to Markdown format"""
    
    # Find all tables
    table_pattern = re.compile(r'<table[^>]*>(.*?)</table>', re.DOTALL)
    
    def convert_single_table(match):
        table_html = match.group(1)
        
        # Extract caption if present
        caption_match = re.search(r'<caption[^>]*>(.*?)</caption>', table_html, re.DOTALL)
        caption = caption_match.group(1) if caption_match else ""
        
        # Extract headers
        thead_match = re.search(r'<thead[^>]*>(.*?)</thead>', table_html, re.DOTALL)
        headers = []
        if thead_match:
            header_rows = re.findall(r'<tr[^>]*>(.*?)</tr>', thead_match.group(1), re.DOTALL)
            for row in header_rows:
                cells = re.findall(r'<th[^>]*>(.*?)</th>', row, re.DOTALL)
                headers.extend([re.sub(r'<[^>]+>', '', cell).strip() for cell in cells])
        
        # Extract body rows
        tbody_match = re.search(r'<tbody[^>]*>(.*?)</tbody>', table_html, re.DOTALL)
        if not tbody_match:
            tbody_match = re.search(r'<table[^>]*>(.*?)</table>', table_html, re.DOTALL)
        
        rows = []
        if tbody_match:
            body_rows = re.findall(r'<tr[^>]*>(.*?)</tr>', tbody_match.group(1), re.DOTALL)
            for row in body_rows:
                cells = re.findall(r'<td[^>]*>(.*?)</td>', row, re.DOTALL)
                if cells:  # Only add rows with actual data
                    cleaned_cells = [re.sub(r'<[^>]+>', '', cell).strip() for cell in cells]
                    rows.append(cleaned_cells)
        
        # Build Markdown table
        if not headers and not rows:
            return ""
        
        markdown_table = ""
        if caption:
            caption_clean = re.sub(r'<[^>]+>', '', caption).strip()
            markdown_table += f"**{caption_clean}**\n\n"
        
        # Use headers if available, otherwise use first row as headers
        if headers:
            markdown_table += "| " + " | ".join(headers) + " |\n"
            markdown_table += "|" + "---|" * len(headers) + "\n"
        elif rows:
            headers = rows.pop(0)
            markdown_table += "| " + " | ".join(headers) + " |\n"
            markdown_table += "|" + "---|" * len(headers) + "\n"
        
        # Add data rows
        for row in rows:
            if len(row) >= len(headers if headers else []):
                markdown_table += "| " + " | ".join(row[:len(headers) if headers else len(row)]) + " |\n"
        
        return markdown_table + "\n"
    
    return table_pattern.sub(convert_single_table, content)

def main():
    if len(sys.argv) != 3:
        print("Usage: python convert_html_to_md.py input.html output.md")
        sys.exit(1)
    
    input_file = Path(sys.argv[1])
    output_file = Path(sys.argv[2])
    
    if not input_file.exists():
        print(f"Input file {input_file} does not exist")
        sys.exit(1)
    
    # Read HTML content
    with open(input_file, 'r', encoding='utf-8') as f:
        html_content = f.read()
    
    # Convert to Markdown
    markdown_content = clean_html_content(html_content)
    
    # Write output
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(markdown_content)
    
    print(f"Converted {input_file} to {output_file}")

if __name__ == "__main__":
    main()