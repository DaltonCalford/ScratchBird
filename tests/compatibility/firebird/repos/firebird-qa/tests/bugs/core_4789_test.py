#coding:utf-8

"""
ID:          issue-5088
ISSUE:       5088
TITLE:       Prohibit ability to cast timestamps that out of valid range to varchar
DESCRIPTION:
JIRA:        CORE-4789
FBTEST:      bugs.core_4789
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    --set echo on;

    -- Initial checks (from tracker stat post):

    select cast(dateadd( 87649415 hour to timestamp '01.01.0001 00:00:00.0000') as varchar(32)) text_dts_1
    from rdb$database;

    select cast(dateadd(  87649415000 hour to timestamp '01.01.0001 00:00:00.0000') as varchar(32))  text_dts_2a
    from rdb$database;

    select cast(dateadd( -87649415000 hour to timestamp '31.12.9999 23:59:59.9999') as varchar(32))  text_dts_2b
    from rdb$database;


    select cast(dateadd(  8764941500000 hour to timestamp '01.01.0001 00:00:00.0000') as varchar(32))  text_dts_3a
    from rdb$database;

    select cast(dateadd( -8764941500000 hour to timestamp '31.12.9999 23:59:59.9999') as varchar(32))  text_dts_3b
    from rdb$database;

    -- Additional checks for boundary values:
    set term ^;
    execute block as
    begin
      rdb$set_context('USER_SESSION','MAX_DIFF_IN_MS', datediff(millisecond from timestamp '01.01.0001 00:00:00.000' to timestamp '31.12.9999 23:59:59.999' ));
    end
    ^ set term ;^


    -- Jump forward exactly to 31.12.9999 23:59:59.999
    select cast( dateadd( rdb$get_context('USER_SESSION','MAX_DIFF_IN_MS') millisecond to timestamp '01.01.0001 00:00:00.000') as varchar(32)) text_dts4
    from rdb$database;

    -- Jump forward beyond 31.12.9999 23:59:59.999  (i.e. to 01.01.10000 00:00:00.001):
    select cast( dateadd( 1+cast(rdb$get_context('USER_SESSION','MAX_DIFF_IN_MS') as bigint) millisecond to timestamp '01.01.0001 00:00:00.000') as varchar(32)) text_dts5
    from rdb$database;

    -- Jump backward exactly to 01.01.0001 00:00:00.000
    select cast( dateadd( -cast(rdb$get_context('USER_SESSION','MAX_DIFF_IN_MS') as bigint) millisecond to timestamp '31.12.9999 23:59:59.999') as varchar(32)) text_dts6
    from rdb$database;

    -- Jump backward beyond 01.01.0001 00:00:00.000 (i.e. to 01.01.0000 23:59:59.999):
    select cast( dateadd( -cast(rdb$get_context('USER_SESSION','MAX_DIFF_IN_MS') as bigint)-1 millisecond to timestamp '31.12.9999 23:59:59.999') as varchar(32)) text_dts7
    from rdb$database;


    -- Check postfix ( http://sourceforge.net/p/firebird/code/61565 ):
    select
         cast( dateadd( 1 second to cast('00:00:00' as time) ) as varchar(15))     text_tm1
        ,cast( dateadd( 86400 second to cast('00:00:00' as time) ) as varchar(15)) text_tm2
        ,cast( dateadd(-1 second to cast('00:00:00' as time) ) as varchar(15))     text_tm3
        ,cast( dateadd(-86400 second to cast('00:00:00' as time) ) as varchar(15)) text_tm4
    from rdb$database rows 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TEXT_DTS_1                      9999-12-31 23:00:00.0000
    TEXT_DTS4                       9999-12-31 23:59:59.9990
    TEXT_DTS6                       0001-01-01 00:00:00.0000
    TEXT_TM1                        00:00:01.0000
    TEXT_TM2                        00:00:00.0000
    TEXT_TM3                        23:59:59.0000
    TEXT_TM4                        00:00:00.0000
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid timestamps
    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid timestamps
    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid timestamps
    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid timestamps
    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid timestamps
    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid timestamps
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

