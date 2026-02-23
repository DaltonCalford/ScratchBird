#coding:utf-8

"""
ID:          issue-6308
ISSUE:       6308
TITLE:       Change behavior of skipped and repeated wall times within time zones
DESCRIPTION:
JIRA:        CORE-6058
FBTEST:      bugs.core_6058
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Change behavior of skipped and repeated wall times within time zones
    -- Within time zones, some wall times does not exist (DST starting) or repeats twice (DST ending).

    set heading off;

    -- is repeated twice (fall backward), but it must be interpreted as 1:30 AM UTC-04 instead of 1:30 AM UTC-05"
    select timestamp '2017-11-05 01:30 America/New_York' from rdb$database;
    select timestamp '2017-11-05 01:30 -04' from rdb$database;
    select datediff(
        second from
        timestamp '2017-11-05 01:30 America/New_York'
        to
        timestamp '2017-11-05 01:30 -04'
    ) from rdb$database;


    -- does not exist, but it must be interpreted as 2:30 AM UTC-05 (equivalent to 3:30 AM UTC-04)"
    select timestamp '2017-03-12 02:30 America/New_York' from rdb$database;
    select timestamp '2017-03-12 02:30 -05' from rdb$database;
    select datediff(
        second from
        timestamp '2017-03-12 02:30 America/New_York'
        to
        timestamp '2017-03-12 02:30 -05'
    ) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    2017-11-05 01:30:00.0000 America/New_York
    2017-03-12 03:30:00.0000 America/New_York
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22009
    Invalid time zone offset: -04 - must use format +/-hours:minutes and be between -14:00 and +14:00
    Statement failed, SQLSTATE = 22009
    Invalid time zone offset: -04 - must use format +/-hours:minutes and be between -14:00 and +14:00
    Statement failed, SQLSTATE = 22009
    Invalid time zone offset: -05 - must use format +/-hours:minutes and be between -14:00 and +14:00
    Statement failed, SQLSTATE = 22009
    Invalid time zone offset: -05 - must use format +/-hours:minutes and be between -14:00 and +14:00
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
