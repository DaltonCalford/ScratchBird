#coding:utf-8

"""
ID:          issue-5170
ISSUE:       5170
TITLE:       Infinite "similar to" matching
DESCRIPTION:
JIRA:        CORE-4874
FBTEST:      bugs.core_4874
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;

    set term ^;
    execute block as
    begin
        rdb$set_context( 'USER_SESSION','DTS_START', cast('now' as timestamp) );
        rdb$set_context( 'USER_SESSION','MAX_THRESHOLD_MS',  500 );
        --                                                    ^
        --                                                    |
        --                                      #############################
        --                                      MAX ALLOWED EXECUTION TIME,MS
        --                                      #############################
    end
    ^
    set term ;^

    select 1 from rdb$database
    where
    '<tr style="height: 1px"><td width="11"/><td width="17"/><td width="43"/><td width="35"/><td width="30"/><td width="125"/><td width="4"/><td width="49"/><td width="83"/><td width="136"/><td width="117"/><td width="8"/><td width="4"/><td width="53"/><td width="13"/><td width="21"/><td width="7"/></tr>'
    similar to '%>(_%_){0,6}</td>' escape '\\'
    --                                     ^
    --                                     |
    --                  ATTENTION: TWO BACKSLASHES MUST BE HERE WHEN USE FBT_RUN TO CHECK
    --                             ###############
    ;

    set count off;
    select
        iif( evaluated_ms <= max_allowed_ms
            ,'acceptable'
            ,'TOO LONG: ' || evaluated_ms || ' ms - this is more then threshold = ' || max_allowed_ms || ' ms'
           ) as duration
    from (
        select
             datediff( millisecond from cast(rdb$get_context( 'USER_SESSION','DTS_START') as timestamp) to current_timestamp ) evaluated_ms
            ,cast( rdb$get_context( 'USER_SESSION','MAX_THRESHOLD_MS' ) as int ) as max_allowed_ms
        from rdb$database
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
    DURATION                        acceptable
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

