#coding:utf-8

"""
ID:          issue-1780
ISSUE:       1780
TITLE:       Too large numbers cause positive infinity to be inserted into database
DESCRIPTION:
JIRA:        CORE-1362
FBTEST:      bugs.core_1362
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

test_script_1 = """
    recreate table test (col1 double precision);
    commit;
    -- this should PASS:
    insert into test values( 1.79769313486231570E+308 );
    -- this is too big, should raise exception:
    insert into test values( 1.79769313486232e+308 );
    commit;
"""

act_1 = isql_act('db', test_script_1)

expected_stderr_1 = """
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stderr = expected_stderr_1
    act_1.execute()
    assert act_1.clean_stderr == act_1.clean_expected_stderr

# version: 4.0

test_script_2 = """
    recreate table test (col1 double precision);
    commit;
    -- this should PASS:
    insert into test values( 1.79769313486231570E+308 );
    -- this is too big, should raise exception:
    insert into test values( 1.79769313486232e+308 );
    commit;
"""

act_2 = isql_act('db', test_script_2)

expected_stderr_2 = """
    Statement failed, SQLSTATE = 22003
    Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stderr = expected_stderr_2
    act_2.execute()
    assert act_2.clean_stderr == act_2.clean_expected_stderr

