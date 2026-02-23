#coding:utf-8

"""
ID:          issue-4198
ISSUE:       4198
TITLE:       Very poor performance of SIMILAR TO on some arguments + unable to disconnect via DELETE FROM MON$ATTACHMENTS
DESCRIPTION:
  Confirmed normal work (evaluation for less than 2 ms) on WI-T4.0.0.1598.
  Note. Part of pattern: "[[:ALNUM:]\\_\\-]" -- looks strange but it is correct.
  And it should NOT be changed to somewhat like "[[:ALNUM:]]\\_\\-": previoud FB versions hanged exactly
  because of this "strange and wrong" pattern.
JIRA:        CORE-3858
FBTEST:      bugs.core_3858
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

    select 
        iif('sdf--asdf_sd.dsfsfh-.sdjskdjfh-_.' 
        SIMILAR TO 
        '(([[:ALNUM:]\_\-])+.?)+([[:ALNUM:]\_\-])+' escape '\\', 1, 0) as result from rdb$database
    --                                                      ^
    --                                                      |
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
    RESULT                          0
    Records affected: 1
    DURATION                        acceptable
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

