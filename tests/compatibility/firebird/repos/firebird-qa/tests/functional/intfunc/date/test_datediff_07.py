#coding:utf-8

"""
ID:          intfunc.date.datediff-07
TITLE:       DATEDIFF
DESCRIPTION:
  Returns an exact numeric value representing the interval of time from the first date/time/timestamp value to the second one.
FBTEST:      functional.intfunc.date.datediff_07
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set list on;
    select datediff( millisecond,
                     cast( '01.01.0001 00:00:00.0001' as timestamp),
                     cast( '31.12.9999 23:59:59.9999' as timestamp)
                   ) as dd_01a from rdb$database;

    select datediff( millisecond,
                     time '00:00:00.0001',
                     time '23:59:59.9999'
                   ) as dd_01b from rdb$database;


    select datediff( millisecond
                     from cast( '01.01.0001 00:00:00.0001' as timestamp)
                     to cast( '31.12.9999 23:59:59.9999' as timestamp)
                   ) as dd_02a from rdb$database;

    select datediff( millisecond
                     from cast( '00:00:00.0001' as time)
                     to cast( '23:59:59.9999' as time)
                   ) as dd_02b from rdb$database;

"""

act = isql_act('db', test_script)

expected_stdout = """
    DD_01A                          315537897599999.8
    DD_01B                          86399999.8
    DD_02A                          315537897599999.8
    DD_02B                          86399999.8
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
