#coding:utf-8

"""
ID:          issue-6482
ISSUE:       6482
TITLE:       DECFLOAT: subtraction Num1 - Num2 leads to "Decimal float overflow" if Num2
  is specified in scientific notation and less than max double ( 1.7976931348623157e308 )
DESCRIPTION:
JIRA:        CORE-6238
FBTEST:      bugs.core_6238
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- All following statements raised before fix:
    -- Statement failed, SQLSTATE = 22003
    -- Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.

    set list on;
    select (d - 1e0) as result_01 from (select 9.999999999999999999999999999999998E+6144 d from rdb$database);
    select (d - 1.79769313e308) as result_02 from (select 9.999999999999999999999999999999998E+6144 d from rdb$database);
    select (d - cast( 1e0 as float) ) as result_03  from (select 9.999999999999999999999999999999998E+6144 d from rdb$database);
    select (d + 1.79769313e-308 ) as result_04  from (select 9.999999999999999999999999999999998E+6144 d from rdb$database);

    -- This EB was added only to check that no error will be while executing statements in it.
    -- See letter to Alex, 31.01.2020 11:34, and his postfix for this ticket:
    -- https://github.com/FirebirdSQL/firebird/commit/0ef5a1a1c1bf42021b378e1691aaccfd75a454b4
    set term ^;
    execute block as
        declare dt date;
        declare tm time;
        declare ts timestamp;
    begin
        select current_date + cast(1 as numeric(19,0)) from rdb$database into dt;
        select current_date + cast(1 as decfloat) from rdb$database into dt;

        select current_time + cast(1 as numeric(19,0)) from rdb$database into tm;
        select current_time + cast(1 as decfloat) from rdb$database into tm;

        select current_timestamp + cast(1 as decfloat) from rdb$database into ts;
        select current_timestamp + cast(1 as decfloat) from rdb$database into ts;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT_01                        9.999999999999999999999999999999998E+6144
    RESULT_02                        9.999999999999999999999999999999998E+6144
    RESULT_03                        9.999999999999999999999999999999998E+6144
    RESULT_04                        9.999999999999999999999999999999998E+6144
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
