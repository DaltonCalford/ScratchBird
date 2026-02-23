#coding:utf-8

"""
ID:          computed-fields-09
FBTEST:      functional.gtcs.computed_fields_09
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_09.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set heading off;

    -- Test verifies COMPUTED-BY field which expression uses CAST
    -- of miscelaneous non-text datatypes to char and vice versa.

    /*-----------------------------------*/
    /* Computed by CAST(char as integer) */
    /*-----------------------------------*/
    create table t0 (f_char char(25), char_as_int computed by (cast(f_char as integer)));
    commit; -- t0;
    insert into t0(f_char) values('10');
    insert into t0(f_char) values('11');
    select 'Passed 1 - Insert' from t0 where char_as_int = cast(f_char as integer) having count(*) = 2;

    update t0 set f_char = '12' where f_char = '10';
    select 'Passed 1 - Update' from t0 where char_as_int = cast(f_char as integer) having count(*) = 2;

    /*---------------------------------*/
    /* Computed by CAST(char as float) */
    /*---------------------------------*/
    create table t5 (f_char char(25), char_as_float computed by (cast(f_char as float)));
    commit; -- t5;
    insert into t5(f_char) values('10.12');
    insert into t5(f_char) values('11.12');
    select 'Passed 2 - Insert' from t5 where char_as_float = cast(f_char as float) having count(*) = 2;

    update t5 set f_char = '12.12' where f_char = '10.12';
    select 'Passed 2 - Update' from t5 where char_as_float = cast(f_char as float) having count(*) = 2;

    /*--------------------------------*/
    /* Computed by CAST(char as date) */
    /*--------------------------------*/
    create table t10(f_char char(25), char_as_date computed by (cast(f_char as date)));
    commit; -- t10;
    insert into t10(f_char) values('01/01/93');
    insert into t10(f_char) values('02/01/95');
    select 'Passed 3 - Insert' from t10 where char_as_date = cast(f_char as date) having count(*) = 2;

    update t10 set f_char = '02/01/94' where f_char = '01/01/93';
    select 'Passed 3 - Update' from t10 where char_as_date = cast(f_char as date) having count(*) = 2;

    /*--------------------------------*/
    /* Computed by CAST(date as char) */
    /*--------------------------------*/
    create table t15(f_date date, date_as_char computed by (cast(f_date as char(15))));
    commit; -- t15;
    insert into t15(f_date) values('today');
    insert into t15(f_date) values('tomorrow');
    select 'Passed 4 - Insert' from t15 where date_as_char = cast(f_date as char(15)) having count(*) = 2;

    update t15 set f_date = 'yesterday' where f_date = 'today';
    select 'Passed 4 - Update' from t15 where date_as_char = cast(f_date as char(15)) having count(*) = 2;

    /*-------------------------------*/
    /* Computed by CAST(int as char) */
    /*-------------------------------*/
    create table t20(f_int integer, int_as_char computed by (cast(f_int as char(15))));
    commit; -- t20;
    insert into t20(f_int) values(10);
    insert into t20(f_int) values(11);
    select 'Passed 5 - Insert' from t20 where int_as_char = cast(f_int as char(15)) having count(*) = 2;

    update t20 set f_int = 12 where f_int = 10;
    select 'Passed 5 - Update' from t20 where int_as_char = cast(f_int as char(15)) having count(*) = 2;

    /*---------------------------------*/
    /* Computed by CAST(float as char) */
    /*---------------------------------*/
    create table t25(f_float float, float_as_char computed by (cast(f_float as char(15))));
    commit; -- t25;
    insert into t25(f_float) values(10.12);
    insert into t25(f_float) values(11.12);
    select 'Passed 6 - Insert' from t25 where float_as_char = cast(f_float as char(15)) having count(*) = 2;

    update t25 set f_float = 12.12 where f_float = 10.12;
    select 'Passed 6 - Update' from t25 where float_as_char = cast(f_float as char(15)) having count(*) = 2;

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
