#coding:utf-8

"""
ID:          issue-7761
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7761
TITLE:       Regression when displaying line number of errors in ISQL scripts
DESCRIPTION:
NOTES:
    [28.09.2023] pzotov
    Confirmed bug on 6.0.0.49.
    Checked on 6.0.0.55, 5.0.0.1229, 4.0.0.2995 - all fine.

    NB-1: bug also presents in 3.0.12.33713, thus min_version of this test is 4.0.
    NB-2: SQL script has to be save in the file and ISQL must run with command key '-i <script>' rather than via PIPE in order to see bug.
"""

from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions = [('(At|After) line 1 in file.*', 'At line 1 in file'), ('-Token unknown .*', '-Token unknown')])

f_tmp_sql = temp_file('tmp_gh_7761.sql')

@pytest.mark.version('>=4.0')
def test_1(act: Action, f_tmp_sql: Path, capsys):
    with open(f_tmp_sql, 'w') as f:
        f.write("create table t0 (a integer, af computed by (a*3) default 10);")

    expected_stdout = """
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Token unknown - line 1111, column 50111
        -default
        At line 1 in file
    """
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q', '-i', str(f_tmp_sql)], combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
