#coding:utf-8

"""
ID:          issue-2991
ISSUE:       2991
TITLE:       Infinity should not escape from the engine
DESCRIPTION:
JIRA:        CORE-2581
FBTEST:      bugs.core_2581
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """set sql dialect 1;
select 1e161/1e-161from rdb$database;

set sql dialect 3;
select 1e308 + 1e308 from rdb$database;
select 1e308 - -1e308 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """WARNING: Client SQL dialect has been set to 1 when connecting to Database SQL dialect 3 database.

                 DIVIDE
=======================

                    ADD
=======================

               SUBTRACT
=======================
"""

expected_stderr = """Statement failed, SQLSTATE = 22003

arithmetic exception, numeric overflow, or string truncation

-Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.

Statement failed, SQLSTATE = 22003

arithmetic exception, numeric overflow, or string truncation

-Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.

Statement failed, SQLSTATE = 22003

arithmetic exception, numeric overflow, or string truncation

-Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

