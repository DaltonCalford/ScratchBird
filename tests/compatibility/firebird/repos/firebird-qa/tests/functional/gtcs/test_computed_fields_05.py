#coding:utf-8

"""
ID:          computed-fields-05
FBTEST:      functional.gtcs.computed_fields_05
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_05.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;

    /*
    ** Syntax test cases - Valid DATE operations
    */

    /*-----------------*/
    /* Computed by (d) */
    /*-----------------*/
    create table t0 (d date, dc computed by (d));
    commit; -- t0;
    insert into t0(d) values('today');
    insert into t0(d) values('tomorrow');
    insert into t0(d) values('yesterday');
    insert into t0(d) values('now');
    select 'Passed 1 - Insert' from t0 where dc = d having count(*) = 4;

    /*----------------------------------------------------------------*/
    /* Note: Using cast() to convert 'today' first to a date literal. */
    /*       And, then doing date arithmetic. The proper behaviour    */
    /*       should just allow straight date addition as              */
    /*       set d = 'today' + 5. There is already a bug entered about*/
    /*       this (Bug No. xxxx). Change this test case once this     */
    /*       bug is fixed.                                            */
    /*----------------------------------------------------------------*/
    update t0 set d = cast('today'as date) + 5  where d = 'today';
    select 'Passed 1 - Update' from t0 where dc = d having count(*) = 4;

    /*---------------------*/
    /* Computed by (d + 1) */
    /*---------------------*/
    create table t5 (d date, dc computed by (d + 1));
    commit; -- t5;
    insert into t5(d) values('today');
    insert into t5(d) values('tomorrow');
    insert into t5(d) values('yesterday');
    insert into t5(d) values('now');
    select 'Passed 2 - Insert' from t5 where dc = d + 1 having count(*) = 4;

    update t5 set d = cast('today' as date) + 5 where d = 'today';
    select 'Passed 2 - Update' from t5 where dc = d + 1 having count(*) = 4;

    /*---------------------*/
    /* Computed by (d - 1) */
    /*---------------------*/
    create table t10 (d date, dc computed by (d - 1));
    commit; -- t10;
    insert into t10(d) values('today');
    insert into t10(d) values('tomorrow');
    insert into t10(d) values('yesterday');
    insert into t10(d) values('now');
    select 'Passed 3 - Insert' from t10 where dc = d - 1 having count(*) = 4;

    update t10 set d = cast('today' as date) - 5 where d = 'today';
    select 'Passed 3 - Update' from t10 where dc = d - 1 having count(*) = 4;

    /*-----------------------*/
    /* Computed by ('today') */
    /*-----------------------*/
    create table t15 (d date, dc computed by ('today'));
    commit; -- t15;
    insert into t15(d) values('today');
    insert into t15(d) values('tomorrow');
    insert into t15(d) values('yesterday');
    insert into t15(d) values('now');
    select 'Passed 4 - Insert' from t15 where dc = 'today' having count(*) = 4;

    update t15 set d = cast('today' as date) + 5 where d = 'today';
    select 'Passed 4 - Update' from t15 where dc = 'today' having count(*) = 4;

    /*---------------------------*/
    /* Computed by ('today' + 1) */
    /*---------------------------*/
    create table t20 (d date, dc computed by (cast('today' as date) + 1 ));
    commit; -- t20;
    insert into t20(d) values('today');
    insert into t20(d) values('tomorrow');
    insert into t20(d) values('yesterday');
    insert into t20(d) values('now');
    select 'Passed 5 - Insert' from t20 where dc = cast('today' as date) + 1 having count(*) = 4;

    update t20 set d = cast('today' as date) + 5 where d = 'today';
    select 'Passed 5 - Update' from t20 where dc = cast('today' as date) + 1 having count(*) = 4;

    /*-----------------------*/
    /* Computed by (d1 - d2) */
    /*-----------------------*/
    create table t25 (d_start date, d_end date, date_diff computed by (d_end - d_start));
    commit; -- t25;
    insert into t25(d_start, d_end) values('yesterday', 'today');
    insert into t25(d_start, d_end) values('today', 'tomorrow');
    insert into t25(d_start, d_end) values('yesterday', 'tomorrow');
    select 'Passed 6 - Insert' from t25 where date_diff = d_end - d_start having count(*) = 3;

    update t25 set d_end = cast('today' as date) + 5 where d_start = 'today';
    select 'Passed 6 - Update' from t25 where date_diff = d_end - d_start having count(*) = 3;

"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    Passed 1 - Insert
    Passed 1 - Update
    Passed 2 - Insert
    Passed 2 - Update
    Passed 3 - Insert
    Passed 3 - Update
    Passed 4 - Insert
    Passed 4 - Update
    Passed 5 - Insert
    Passed 5 - Update
    Passed 6 - Insert
    Passed 6 - Update
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
