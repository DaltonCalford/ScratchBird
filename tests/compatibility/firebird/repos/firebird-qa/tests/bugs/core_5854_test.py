#coding:utf-8

"""
ID:          issue-6114
ISSUE:       6114
TITLE:       Very poor "similar to" performance
DESCRIPTION:
  Quantifier value was increased to 1000, string 'abcd.abc' is used for pattern search.
NOTES:
  doc\\sql.extensions\\README.similar_to.txt:
  Since FB 4 the repeat factor low/high values could not be greater than 1000.
JIRA:        CORE-5854
FBTEST:      bugs.core_5854
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
        rdb$set_context( 'USER_SESSION','MAX_THRESHOLD_MS', 100 );
        --                                                    ^
        --                                                    |
        --                                      #############################
        --                                      MAX ALLOWED EXECUTION TIME,MS
        --                                      #############################
    end
    ^
    set term ;^

    select iif ('abcd.abc' similar to '[[:ALPHA:].]{1,1000}.ab' ,1,0) result
    from rdb$database
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
