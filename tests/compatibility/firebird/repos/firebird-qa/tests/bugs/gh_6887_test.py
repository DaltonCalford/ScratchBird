#coding:utf-8

"""
ID:          issue-6887
ISSUE:       6887
TITLE:       Invalid SIMILAR TO patterns may lead memory read beyond string limits
DESCRIPTION:
    On 5.0.0.88 and 4.0.1.2523 expression marked as [ 2 ] raises:
    "SQLSTATE = 22025/Invalid ESCAPE sequence",
    After fix its error became the same as for [ 1 ].

    NB: backslash character must be duplicated when SQL script is launched from Python,
    in contrary to common usage (pass script to ISQL utility from OS command prompt).
FBTEST:      bugs.gh_6887
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select '1' similar to '1[a-' from rdb$database; ----------------------- [ 1 ]

    -- NOTE: we have to DUPLICATE backslash here otherwise Python
    -- framework will 'swallow' it and error message will differ.
    -- Single backslash must be used if this expression is passed
    -- to ISQL from OS command prompt or using '-i' command switch:
    select '1' similar to '1\\' escape '\\' from rdb$database; ------------ [ 2 ]
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Invalid SIMILAR TO pattern

    Statement failed, SQLSTATE = 42000
    Invalid SIMILAR TO pattern
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
