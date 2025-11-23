#!/usr/bin/env python3
"""
Convert standalone test files to GoogleTest format.

This script converts test files that use:
- int main() { ... }
- assert(...) statements
- std::cout for output

To GoogleTest format with:
- TEST(TestSuiteName, TestName) { ... }
- ASSERT_*/EXPECT_* macros
- Proper test organization
"""

import re
import sys
from pathlib import Path

def extract_test_name_from_file(file_path):
    """Extract a suitable test suite name from the file path."""
    # e.g., test_array.cpp -> ArrayTest
    name = Path(file_path).stem
    if name.startswith('test_'):
        name = name[5:]  # Remove 'test_' prefix
    # Convert snake_case to CamelCase
    parts = name.split('_')
    camel = ''.join(p.capitalize() for p in parts)
    return camel + 'Test'

def replace_assert(match):
    """Replace assert() with appropriate GoogleTest macro."""
    assertion = match.group(1).strip()

    # Simple patterns for has_value()
    if '.has_value()' in assertion and assertion.startswith('!'):
        return f'ASSERT_FALSE({assertion[1:].strip()});'
    elif '.has_value()' in assertion:
        return f'ASSERT_TRUE({assertion});'

    # Check for equality (must be careful with -> and method calls)
    if ' == ' in assertion:
        parts = assertion.split(' == ', 1)
        if len(parts) == 2:
            return f'ASSERT_EQ({parts[0].strip()}, {parts[1].strip()});'

    # Check for inequality
    if ' != ' in assertion:
        parts = assertion.split(' != ', 1)
        if len(parts) == 2:
            return f'ASSERT_NE({parts[0].strip()}, {parts[1].strip()});'

    # Check for less than (but not <<)
    if ' < ' in assertion and ' << ' not in assertion:
        parts = assertion.split(' < ', 1)
        if len(parts) == 2:
            return f'ASSERT_LT({parts[0].strip()}, {parts[1].strip()});'

    # Check for greater than (but not >>)
    if ' > ' in assertion and ' >> ' not in assertion:
        parts = assertion.split(' > ', 1)
        if len(parts) == 2:
            return f'ASSERT_GT({parts[0].strip()}, {parts[1].strip()});'

    # Check for <=
    if ' <= ' in assertion:
        parts = assertion.split(' <= ', 1)
        if len(parts) == 2:
            return f'ASSERT_LE({parts[0].strip()}, {parts[1].strip()});'

    # Check for >=
    if ' >= ' in assertion:
        parts = assertion.split(' >= ', 1)
        if len(parts) == 2:
            return f'ASSERT_GE({parts[0].strip()}, {parts[1].strip()});'

    # Check for logical NOT
    if assertion.startswith('!'):
        return f'ASSERT_FALSE({assertion[1:].strip()});'

    # Default: ASSERT_TRUE
    return f'ASSERT_TRUE({assertion});'

def convert_file(input_path, output_path=None):
    """Convert a standalone test file to GoogleTest format."""
    if output_path is None:
        output_path = input_path

    with open(input_path, 'r') as f:
        content = f.read()

    # Replace cassert with gtest
    content = re.sub(r'#include <cassert>', '#include "gtest/gtest.h"', content)

    test_suite_name = extract_test_name_from_file(input_path)

    # Try multiple patterns for finding main()
    # Pattern 1: Simple main with return 0 directly
    main_match = re.search(r'int main\(\s*\)\s*\{(.*?)\n\s*return 0;?\s*\n\}', content, re.DOTALL)

    # Pattern 2: main with try-catch
    if not main_match:
        main_match = re.search(r'int main\(\s*\)\s*\{(.*)\}(?:\s*$)', content, re.DOTALL)

    if not main_match:
        print(f"Warning: Could not find main() in {input_path}")
        return False

    main_body = main_match.group(1)
    pre_main = content[:main_match.start()]
    post_main = content[main_match.end():]

    # If main body contains try-catch with individual test function calls,
    # we should keep them but wrap them in TEST() macros

    # Check if this file calls test functions (test_xxx_yyy())
    test_function_calls = re.findall(r'(\w+)\(\);', main_body)
    test_functions = [f for f in test_function_calls if f.startswith('test_')]

    # Replace assert() with appropriate macros
    main_body = re.sub(r'assert\s*\(([^;]+?)\);', replace_assert, main_body)

    test_name = "Comprehensive"

    # If we found test function calls, create a single TEST that calls them
    # Otherwise, inline the entire main body
    if test_functions:
        # Keep the test function calls
        test_content = f"""TEST({test_suite_name}, {test_name}) {{
{main_body}
}}"""
    else:
        test_content = f"""TEST({test_suite_name}, {test_name}) {{
{main_body}
}}"""

    # Create the GoogleTest version
    gtest_content = f"""{pre_main}
{test_content}
{post_main}"""

    # Write the converted file
    with open(output_path, 'w') as f:
        f.write(gtest_content)

    print(f"Converted {input_path} -> {output_path}")
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: python convert_tests_to_gtest.py <test_file.cpp> [output_file.cpp]")
        print("   or: python convert_tests_to_gtest.py <directory>")
        sys.exit(1)

    input_path = Path(sys.argv[1])

    if input_path.is_dir():
        # Convert all .cpp files in directory
        cpp_files = list(input_path.glob('*.cpp'))
        print(f"Found {len(cpp_files)} test files in {input_path}")
        for cpp_file in cpp_files:
            try:
                convert_file(cpp_file)
            except Exception as e:
                print(f"Error converting {cpp_file}: {e}")
    else:
        # Convert single file
        output_path = Path(sys.argv[2]) if len(sys.argv) > 2 else None
        convert_file(input_path, output_path)

if __name__ == '__main__':
    main()
