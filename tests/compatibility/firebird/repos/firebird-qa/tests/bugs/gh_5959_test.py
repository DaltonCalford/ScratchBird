#coding:utf-8

"""
ID:          issue-5959
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5959
TITLE:       Add support for QUARTER to EXTRACT, FIRST_DAY and LAST_DAY [CORE5693]
NOTES:
    [12.05.2023] pzotov
    Checked on 5.0.0.1046
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    select
        dts
        ,extract(quarter from dts ) as extract_quarter
        ,first_day(of quarter from dts) as first_day_of_quarter
        ,last_day(of quarter from dts) as last_day_of_quarter
    from (
        select date '01.01.0001' as dts from rdb$database
    );


    select
        dts
        ,extract(quarter from dts ) as extract_quarter
        ,first_day(of quarter from dts) as first_day_of_quarter
        ,last_day(of quarter from dts) as last_day_of_quarter
    from (
        select timestamp '29.02.2020 19:20:21.223' as dts from rdb$database
    );

    select
        dts
        ,extract(quarter from dts ) as extract_quarter
        ,first_day(of quarter from dts) as first_day_of_quarter
        ,last_day(of quarter from dts) as last_day_of_quarter
    from (
        select cast(null as timestamp) as dts from rdb$database
    );

    select
        dts
        ,extract(quarter from dts ) as extract_quarter
        ,first_day(of quarter from dts) as first_day_of_quarter
        ,last_day(of quarter from dts) as last_day_of_quarter
    from (
        select cast( '2018-01-02 04:04:04.4444 +14:0' as timestamp with time zone) as dts from rdb$database
    );

    select
        dts
        ,extract(quarter from dts ) as extract_quarter
        ,first_day(of quarter from dts) as first_day_of_quarter
        ,last_day(of quarter from dts) as last_day_of_quarter
    from (
        select cast( '2019-03-23 11:31:05.0001 america/new_york' as timestamp with time zone) as dts from rdb$database
    );

"""

act = isql_act('db', test_script)

# NB, doc/sql.extensions/README.builtin_functions.txt:
# "When a timestamp is passed the return value __preserves__ the time part" 
expected_stdout = """
    DTS                             0001-01-01
    EXTRACT_QUARTER                 1
    FIRST_DAY_OF_QUARTER            0001-01-01
    LAST_DAY_OF_QUARTER             0001-03-31

    DTS                             2020-02-29 19:20:21.2230
    EXTRACT_QUARTER                 1
    FIRST_DAY_OF_QUARTER            2020-01-01 19:20:21.2230
    LAST_DAY_OF_QUARTER             2020-03-31 19:20:21.2230

    DTS                             <null>
    EXTRACT_QUARTER                 <null>
    FIRST_DAY_OF_QUARTER            <null>
    LAST_DAY_OF_QUARTER             <null>

    DTS                             2018-01-02 04:04:04.4444 +14:00
    EXTRACT_QUARTER                 1
    FIRST_DAY_OF_QUARTER            2018-01-01 04:04:04.4444 +14:00
    LAST_DAY_OF_QUARTER             2018-03-31 04:04:04.4444 +14:00

    DTS                             2019-03-23 11:31:05.0001 America/New_York
    EXTRACT_QUARTER                 1
    FIRST_DAY_OF_QUARTER            2019-01-01 11:31:05.0001 America/New_York
    LAST_DAY_OF_QUARTER             2019-03-31 11:31:05.0001 America/New_York
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
