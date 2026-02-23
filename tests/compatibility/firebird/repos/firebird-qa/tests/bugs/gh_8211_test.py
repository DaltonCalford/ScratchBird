#coding:utf-8

"""
ID:          issue-8211
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8211
TITLE:       DATEADD truncates milliseconds for month and year deltas.
DESCRIPTION:
NOTES:
    [11.08.2024] pzotov
    Confirmed bug on 6.0.0.423
    Checked on intermediate snapshots: 6.0.0.431-16bb157; 5.0.2.1477-c71eb20; 4.0.6.3141
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
        ''||dateadd(0 millisecond to cast('01.01.2001 01:01:01.1111' as timestamp)) a_millisecond,
        ''||dateadd(0 second to cast('01.01.2001 01:01:01.1111' as timestamp)) a_second,
        ''||dateadd(0 minute to cast('01.01.2001 01:01:01.1111' as timestamp)) a_minute,
        ''||dateadd(0 hour to cast('01.01.2001 01:01:01.1111' as timestamp)) a_hour,
        ''||dateadd(0 day to cast('01.01.2001 01:01:01.1111' as timestamp)) a_day,
        ''||dateadd(0 week to cast('01.01.2001 01:01:01.1111' as timestamp)) a_week,
        ''||dateadd(0 month to cast('01.01.2001 01:01:01.1111' as timestamp)) a_month,
        ''||dateadd(0 year to cast('01.01.2001 01:01:01.1111' as timestamp)) a_year
    from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    A_MILLISECOND                   2001-01-01 01:01:01.1111
    A_SECOND                        2001-01-01 01:01:01.1111
    A_MINUTE                        2001-01-01 01:01:01.1111
    A_HOUR                          2001-01-01 01:01:01.1111
    A_DAY                           2001-01-01 01:01:01.1111
    A_WEEK                          2001-01-01 01:01:01.1111
    A_MONTH                         2001-01-01 01:01:01.1111
    A_YEAR                          2001-01-01 01:01:01.1111
"""

@pytest.mark.version('>=4.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
