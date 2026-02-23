#coding:utf-8

"""
ID:          issue-6569
ISSUE:       6569
TITLE:       FB4 Beta 2 may still be using the current date for TIME WITH TIME ZONE and extended wire protocol
DESCRIPTION:
JIRA:        CORE-6328
FBTEST:      bugs.core_6328
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bind of time zone to extended;
    create table test (
        timecol time with time zone
    );

    insert into test(timecol) values('11:31:05.0001 america/new_york');

    set list on;
    select extract(timezone_hour from timecol) as tz_hour, extract(timezone_minute from timecol) as tz_minute
    from test;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    TZ_HOUR                         -5
    TZ_MINUTE                       0
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
