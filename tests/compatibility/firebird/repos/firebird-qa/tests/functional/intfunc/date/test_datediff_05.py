#coding:utf-8

"""
ID:          intfunc.date.datediff-05
TITLE:       DATEDIFF
DESCRIPTION:
  Returns an exact numeric value representing the interval of time from the first date/time/timestamp value to the second one.
FBTEST:      functional.intfunc.date.datediff_05
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select datediff(MONTH,cast( '12/02/2008 13:33:33' as timestamp),cast( '12/02/2009 13:34:35' as timestamp)) from rdb$database;
select datediff(MONTH FROM cast( '12/02/2008 13:33:33' as timestamp) TO cast( '12/02/2009 13:34:35' as timestamp)) from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
             DATEDIFF
=====================
                   12


             DATEDIFF
=====================
                   12
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
