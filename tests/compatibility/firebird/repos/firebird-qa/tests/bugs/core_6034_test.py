#coding:utf-8

"""
ID:          issue-6284
ISSUE:       6284
TITLE:       The original time zone should be set to the current time zone at routine invocation
DESCRIPTION:
JIRA:        CORE-6034
FBTEST:      bugs.core_6034
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns (ts1 varchar(100), ts2 varchar(100))
    as
        declare procedure p0 returns (t1 timestamp with time zone, t2 timestamp with time zone) as
        begin
            set time zone 'America/New_York';
            t1 = current_timestamp;
            set time zone local;
            t2 = current_timestamp;
        end

        declare procedure p1 returns (t1 timestamp with time zone, t2 timestamp with time zone) as
        begin
            set time zone 'America/Los_Angeles';
            execute procedure p0 returning_values t1, t2;
        end
        declare t1 timestamp with time zone;
        declare t2 timestamp with time zone;
    begin
        -- Initial time zone: 'America/Sao_Paulo';
        execute procedure p1 returning_values t1, t2;
        ts1 = substring( cast(t1 as varchar(100)) from 26 );
        ts2 = substring( cast(t2 as varchar(100)) from 26 );
        suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    TS1                             America/New_York
    TS2                             America/Los_Angeles
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
