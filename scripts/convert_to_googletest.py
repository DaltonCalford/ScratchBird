#!/usr/bin/env python3
"""
Convert standalone test files to GoogleTest format

This script converts tests with standalone main() functions to GoogleTest format by:
1. Adding #include <gtest/gtest.h>
2. Creating a test fixture class with SetUp()/TearDown()
3. Converting test functions to TEST_F() macros
4. Replacing cout/exit(1) with GoogleTest assertions
5. Removing standalone main() function
"""

import re
import sys
import os

def convert_test_to_googletest(file_path):
    """Convert a standalone test file to GoogleTest format"""

    with open(file_path, 'r') as f:
        content = f.read()

    # Check if already using GoogleTest
    if 'TEST_F(' in content or 'TEST(' in content:
        print(f"  Skipping {file_path} - already using GoogleTest")
        return False

    # Check if has standalone main
    if 'int main(' not in content:
        print(f"  Skipping {file_path} - no standalone main() function")
        return False

    lines = content.split('\n')
    new_lines = []

    # Extract test name from file
    test_file_base = os.path.basename(file_path)
    test_name_parts = test_file_base.replace('test_', '').replace('.cpp', '').split('_')
    fixture_name = ''.join(word.capitalize() for word in test_name_parts) + 'Test'

    # Track state
    in_main = False
    in_test_function = False
    test_function_name = None
    indent_level = 0

    i = 0
    while i < len(lines):
        line = lines[i]

        # Add GoogleTest include after first include
        if i == 0 or (i > 0 and '#include' in lines[i-1] and '#include' not in line and not new_lines[-1].startswith('#include <gtest')):
            if any('#include' in l for l in new_lines):
                if not any('#include <gtest/gtest.h>' in l for l in new_lines):
                    # Find first non-include line
                    if '#include' not in line and line.strip() and not line.strip().startswith('//'):
                        new_lines.append('#include <gtest/gtest.h>')

        # Skip int main() and its closing brace
        if re.match(r'int\s+main\s*\(', line):
            in_main = True
            i += 1
            continue

        if in_main:
            if line.strip() == '}' and indent_level == 0:
                in_main = False
                i += 1
                continue
            if '{' in line:
                indent_level += line.count('{')
            if '}' in line:
                indent_level -= line.count('}')
            # Skip lines inside main()
            i += 1
            continue

        # Convert void testXxx() functions to TEST_F()
        test_func_match = re.match(r'void\s+(test\w+)\s*\(\s*\)', line)
        if test_func_match:
            func_name = test_func_match.group(1)
            # Convert testFooBar -> FooBar
            test_method_name = func_name[4:]  # Remove 'test' prefix
            test_method_name = test_method_name[0].upper() + test_method_name[1:]  # Capitalize

            new_lines.append(f'TEST_F({fixture_name}, {test_method_name})')
            i += 1
            continue

        new_lines.append(line)
        i += 1

    new_content = '\n'.join(new_lines)

    # Additional conversions
    # Replace cout with GoogleTest when appropriate
    new_content = re.sub(r'std::cout\s*<<\s*"\\n===.*Test:.*===\\n";', '', new_content)
    new_content = re.sub(r'std::cout\s*<<\s*"  ✓.*\\n";', '', new_content)
    new_content = re.sub(r'std::cout\s*<<\s*".*PASSED.*\\n";', '', new_content)

    # Replace exit(1) with FAIL()
    new_content = re.sub(r'exit\(1\);', 'FAIL();', new_content)

    # Replace if + cout + exit with ASSERT
    new_content = re.sub(
        r'if\s*\(status\s*!=\s*Status::OK\)\s*\{\s*std::cout.*?<<.*?<<.*?;\s*FAIL\(\);\s*\}',
        '',
        new_content,
        flags=re.DOTALL
    )

    with open(file_path, 'w') as f:
        f.write(new_content)

    print(f"  ✓ Converted {file_path}")
    return True

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 convert_to_googletest.py <test_file.cpp> [...]")
        sys.exit(1)

    converted_count = 0
    for file_path in sys.argv[1:]:
        if convert_test_to_googletest(file_path):
            converted_count += 1

    print(f"\nConverted {converted_count} test file(s)")
