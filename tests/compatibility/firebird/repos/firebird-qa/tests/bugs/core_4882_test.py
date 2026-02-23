#coding:utf-8

"""
ID:          issue-5176
ISSUE:       5176
TITLE:       ISQL input command (or -i option) reads large (> 64K) lines incorrectly
DESCRIPTION:
  This test verifies ability of parsing multiple statements that are going as `single-lined stream`.
  Source for this test is file `/files/core_4882.sql`.
  It contains almost all source code of test that emulates OLTP workload - without initial script for data filling.
  Three files were taken from it: oltp30_DDL.sql, oltp30_sp.sql and oltp_main_filling.sql - with total size  ~600 Kb.
  Then all single-lined comments ("-- blah blah ...") were removed and remained source code was pulled out in single-line.
  This single-line also could not be compiled up to WI-V3.0.0.31948, raising 'token unknown'.
  No error on compiling should occur since buiild WI-V3.0.0.31981.
  NOTE: before this file also contained lines with bulk of begin..end blocks but since CORE-4884 was fixed that number
  is limited to 512. With this limit single-line statement of begin-end blocks will have length less than 64K. For that
  reason these lines were removed from here to the test for CORE-4884.
JIRA:        CORE-4882
FBTEST:      bugs.core_4882
"""

import pytest
from firebird.qa import *
db = db_factory()

act = python_act('db', substitutions=[('exception [0-9]+', 'exception'),
                                      ('After line [0-9]+ in file.*', ''),
                                      ('CURRENT_TIMESTAMP.*', '')])

expected_stdout = """
    MSG                             oltp30_DDL.sql start
    MSG                             oltp30_DDL.sql finish
    MSG                             oltp30_sp.sql start
    MSG                             oltp30_sp.sql finish
    MSG                             oltp_main_filling.sql start
    MSG                             oltp_main_filling.sql finish
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.isql(switches=[], input_file=act.files_dir / 'core_4882.sql')
    assert act.clean_stdout == act.clean_expected_stdout


