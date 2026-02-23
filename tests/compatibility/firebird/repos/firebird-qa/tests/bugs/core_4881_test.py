#coding:utf-8

"""
ID:          issue-1944
ISSUE:       1944
TITLE:       Increase maximum string literal length to 64K (bytes) while setting a lower
  limit (of characters) for multibyte charsets based on their max char. length
  (UTF-8 literals will be limited to 16383 characters)
DESCRIPTION:
  Test verifies that one may to operate with string literals:
  1) containing only ascii characters (and limit for this case should be 65535 bytes (=chars))
  2) containing unicode characters but all of them requires 3 bytes for encoding (and limit for this should be 16383 character)
  3) containing literals with mixed byte-per-character encoding requirement (limit should be also 16383 character).
  Before 3.0.0.31981 following statement raises:
  String literal with 65536 bytes exceeds the maximum length of 32767 bytes
JIRA:        CORE-4881
FBTEST:      bugs.core_4881
"""

import pytest
from zipfile import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
	O_LEN_ASCII_ONLY                65535
	C_LEN_ASCII_ONLY                65535
	O_LEN_UTF8_3BPC                 49149
	C_LEN_UTF8_3BPC                 16383
	O_LEN_UTF8_MIXED                19889
	C_LEN_UTF8_MIXED                16383
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    script_file = Path(act.files_dir / 'core_4881.zip', at='core_4881_script.sql')
    act.script = script_file.read_text(encoding='utf-8')
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

