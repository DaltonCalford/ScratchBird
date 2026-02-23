#coding:utf-8

"""
ID:          issue-2913
ISSUE:       2913
TITLE:       Binary shift functions give wrong results with negative shift values
DESCRIPTION:
JIRA:        CORE-2501
FBTEST:      bugs.core_2501
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """select bin_shl(100, -1) from rdb$database;
select bin_shr(100, -1) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
              BIN_SHL
=====================

              BIN_SHR
=====================
"""

expected_stderr = """Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Argument for BIN_SHL must be zero or positive

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Argument for BIN_SHR must be zero or positive

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

