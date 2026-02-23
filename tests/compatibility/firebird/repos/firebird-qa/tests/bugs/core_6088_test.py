#coding:utf-8

"""
ID:          issue-6338
ISSUE:       6338
TITLE:       "SIMILAR TO" hangs when processing parenthesis
DESCRIPTION:
JIRA:        CORE-6088
FBTEST:      bugs.core_6088
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
        rdb$set_context( 'USER_SESSION','MAX_THRESHOLD_MS',  150 );
        --                                                    ^
        --                                                    |
        --                                      #############################
        --                                      MAX ALLOWED EXECUTION TIME,MS
        --                                      #############################
    end
    ^
    set term ;^


    select 1 as result from RDB$DATABASE
    where 'a-b c(d)'
    similar to '[[:WHITESPACE:]a-z\\-]{0,199}' escape '\\'
    --                                                ^
    --                                                |
    --                              ATTENTION: TWO BACKSLASHES MUST BE HERE WHEN USE FBT_RUN TO CHECK
    --                                         ###############
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
