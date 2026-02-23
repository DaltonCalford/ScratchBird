#coding:utf-8

"""
ID:          intfunc.date.dateadd-06
TITLE:       DATEADD
DESCRIPTION:
  Returns a date/time/timestamp value increased (or decreased, when negative) by the specified amount of time.
FBTEST:      functional.intfunc.date.dateadd_06
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select dateadd(-1 minute to time '12:12:00' ) as yesterday from rdb$database;
select dateadd(minute,-1, time '12:12:00' ) as yesterday from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
    YESTERDAY
=============
12:11:00.0000


    YESTERDAY
=============
12:11:00.0000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
