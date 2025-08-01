#!/usr/bin/env python3
"""
migrate_fbt_parser.py
FBT (Firebird Test) Parser for ScratchBird Migration

Parses Firebird .fbt test files (Python dictionary format) and extracts
test metadata, SQL scripts, and expected outputs for conversion to 
ScratchBird test format.
"""

import os
import sys
import ast
import json
import re
from pathlib import Path
from typing import Dict, List, Any, Optional

class FBTParser:
    """Parser for Firebird .fbt test files"""
    
    def __init__(self):
        self.stats = {
            'files_processed': 0,
            'files_failed': 0,
            'tests_extracted': 0,
            'versions_found': set(),
            'categories': {}
        }
    
    def parse_fbt_file(self, file_path: str) -> Optional[Dict[str, Any]]:
        """Parse a single .fbt file and extract test data"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Try to evaluate as Python dictionary
            try:
                test_data = ast.literal_eval(content)
            except (SyntaxError, ValueError) as e:
                print(f"Warning: Could not parse {file_path} as Python dict: {e}")
                return None
            
            # Validate structure
            if not isinstance(test_data, dict):
                print(f"Warning: {file_path} does not contain a dictionary")
                return None
            
            # Extract metadata
            parsed_test = {
                'file_path': file_path,
                'file_name': os.path.basename(file_path),
                'id': test_data.get('id', ''),
                'qmid': test_data.get('qmid', ''),
                'tracker_id': test_data.get('tracker_id', ''),
                'title': test_data.get('title', ''),
                'description': test_data.get('description', ''),
                'versions': []
            }
            
            # Process versions
            versions = test_data.get('versions', [])
            if not isinstance(versions, list):
                versions = [versions]
            
            for version in versions:
                if isinstance(version, dict):
                    version_data = {
                        'firebird_version': version.get('firebird_version', ''),
                        'platform': version.get('platform', 'All'),
                        'test_type': version.get('test_type', 'ISQL'),
                        'database': version.get('database', 'Create'),
                        'backup_file': version.get('backup_file', ''),
                        'init_script': version.get('init_script', ''),
                        'test_script': version.get('test_script', ''),
                        'expected_stdout': version.get('expected_stdout', ''),
                        'expected_stderr': version.get('expected_stderr', ''),
                        'substitutions': version.get('substitutions', [])
                    }
                    parsed_test['versions'].append(version_data)
                    self.stats['versions_found'].add(version_data['firebird_version'])
            
            # Update statistics
            self.stats['files_processed'] += 1
            self.stats['tests_extracted'] += len(parsed_test['versions'])
            
            # Categorize test
            category = self._categorize_test(parsed_test['id'])
            if category not in self.stats['categories']:
                self.stats['categories'][category] = 0
            self.stats['categories'][category] += 1
            
            return parsed_test
            
        except Exception as e:
            print(f"Error parsing {file_path}: {e}")
            self.stats['files_failed'] += 1
            return None
    
    def _categorize_test(self, test_id: str) -> str:
        """Categorize test based on ID"""
        if test_id.startswith('bugs.'):
            return 'bug_regression'
        elif 'basic.db' in test_id:
            return 'core_database'
        elif 'domain' in test_id:
            return 'data_types_domains'
        elif 'index' in test_id or 'optimizer' in test_id:
            return 'index_optimization'
        elif 'dml' in test_id or 'view' in test_id or 'procedure' in test_id:
            return 'advanced_sql'
        elif 'intfunc' in test_id:
            return 'builtin_functions'
        elif 'table' in test_id:
            return 'table_operations'
        elif 'database' in test_id:
            return 'database_operations'
        elif 'trigger' in test_id:
            return 'triggers'
        elif 'role' in test_id or 'exception' in test_id:
            return 'security_admin'
        elif 'monitoring' in test_id or 'shadow' in test_id:
            return 'monitoring_admin'
        elif 'generator' in test_id:
            return 'sequences_generators'
        else:
            return 'miscellaneous'
    
    def parse_directory(self, directory: str, pattern: str = "*.fbt") -> List[Dict[str, Any]]:
        """Parse all .fbt files in a directory recursively"""
        directory_path = Path(directory)
        fbt_files = list(directory_path.rglob(pattern))
        
        print(f"Found {len(fbt_files)} .fbt files in {directory}")
        
        parsed_tests = []
        for fbt_file in fbt_files:
            test_data = self.parse_fbt_file(str(fbt_file))
            if test_data:
                parsed_tests.append(test_data)
        
        return parsed_tests
    
    def save_parsed_data(self, parsed_tests: List[Dict[str, Any]], output_file: str):
        """Save parsed test data to JSON file"""
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                # Convert sets to lists for JSON serialization
                stats_copy = dict(self.stats)
                stats_copy['versions_found'] = list(self.stats['versions_found'])
                
                json.dump({
                    'metadata': {
                        'total_tests': len(parsed_tests),
                        'parsing_stats': stats_copy,
                        'versions_found': list(self.stats['versions_found'])
                    },
                    'tests': parsed_tests
                }, f, indent=2, ensure_ascii=False)
            
            print(f"Parsed data saved to: {output_file}")
            
        except Exception as e:
            print(f"Error saving parsed data: {e}")
    
    def print_statistics(self):
        """Print parsing statistics"""
        print("\n" + "="*60)
        print("FBT PARSING STATISTICS")
        print("="*60)
        print(f"Files processed: {self.stats['files_processed']}")
        print(f"Files failed: {self.stats['files_failed']}")
        print(f"Tests extracted: {self.stats['tests_extracted']}")
        print(f"Success rate: {(self.stats['files_processed']/(self.stats['files_processed']+self.stats['files_failed'])*100):.1f}%")
        
        print(f"\nFirebird versions found:")
        for version in sorted(self.stats['versions_found']):
            print(f"  - {version}")
        
        print(f"\nTest categories:")
        for category, count in sorted(self.stats['categories'].items()):
            print(f"  - {category}: {count} tests")

def main():
    """Main function for command-line usage"""
    if len(sys.argv) < 2:
        print("Usage: python3 migrate_fbt_parser.py <OLD_TESTS_DIRECTORY> [output_file.json]")
        print("Example: python3 migrate_fbt_parser.py ../OLD_TESTS_TO_BE_MIGRATED parsed_tests.json")
        sys.exit(1)
    
    input_directory = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else "parsed_fbt_tests.json"
    
    if not os.path.exists(input_directory):
        print(f"Error: Directory {input_directory} does not exist")
        sys.exit(1)
    
    print("FBT Parser - Converting Firebird tests to ScratchBird format")
    print(f"Input directory: {input_directory}")
    print(f"Output file: {output_file}")
    
    parser = FBTParser()
    parsed_tests = parser.parse_directory(input_directory)
    
    if parsed_tests:
        parser.save_parsed_data(parsed_tests, output_file)
        parser.print_statistics()
        
        print(f"\n✅ Successfully parsed {len(parsed_tests)} test files")
        print(f"📁 Results saved to: {output_file}")
    else:
        print("❌ No tests were successfully parsed")
        sys.exit(1)

if __name__ == "__main__":
    main()