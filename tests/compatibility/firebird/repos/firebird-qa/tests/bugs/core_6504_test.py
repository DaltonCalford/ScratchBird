#coding:utf-8

"""
ID:          issue-6734
ISSUE:       6734
TITLE:       Provide same results for date arithmetics when date is changed by values near +/-max(bigint)
DESCRIPTION:
JIRA:        CORE-6504
FBTEST:      bugs.core_6504
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;

    -- All following statements must raise SQLSTATE = 22008.
    -- On LINUX builds before 4.0.0.2437 statements NN 5 and 6
    -- did not raise error. Instead, they issued date = 2020-02-01:

    select 1 as chk_1, date '01.02.2020' +  9223372036854775807 from rdb$database;
    select 2 as chk_2, date '01.02.2020' + -9223372036854775807 from rdb$database;
    select 3 as chk_3, date '01.02.2020' -  9223372036854775807 from rdb$database;
    select 4 as chk_4, date '01.02.2020' - -9223372036854775807 from rdb$database;
    select 5 as chk_5, date '01.02.2020' + -9223372036854775808 from rdb$database;
    select 6 as chk_6, date '01.02.2020' - -9223372036854775808 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid dates

    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid dates

    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid dates

    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid dates

    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid dates

    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid dates
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
