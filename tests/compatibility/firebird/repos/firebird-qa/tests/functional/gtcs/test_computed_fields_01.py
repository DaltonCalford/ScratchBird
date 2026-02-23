#coding:utf-8

"""
ID:          computed-fields-01
FBTEST:      functional.gtcs.computed_fields_01
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_01.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    /*-----------------*/
    /* Computed by (i) */
    /*-----------------*/
    create table t0  (i integer, j computed by (i));
    commit; -- t0;
    insert into t0(i) values(1);
    insert into t0(i) values(2);
    select 'Passed 1 - Insert' from t0 where j = i having count(*) = 2;

    update t0 set i = 99 where i = 2;
    select 'Passed 1 - Update' from t0 where j = i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+i) */
    /*-------------------*/
    create table t5  (i integer, j computed by (i+i));
    commit; -- t5;
    insert into t5(i) values(1);
    insert into t5(i) values(2);
    select 'Passed 2 - Insert' from t5 where j = i+i having count(*) = 2;

    update t5 set i = 99 where i = 2;
    select 'Passed 2 - Update' from t5 where j = i+i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i*i) */
    /*-------------------*/
    create table t10 (i integer, j computed by (i*i));
    commit; -- t10;
    insert into t10(i) values(1);
    insert into t10(i) values(2);
    select 'Passed 3 - Insert' from t10 where j = i*i having count(*) = 2;

    update t10 set i = 99 where i = 2;
    select 'Paseed 3 - Update' from t10 where j = i*i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i/i) */
    /*-------------------*/
    create table t15 (i integer, j computed by (i/i));
    commit; -- t15;
    insert into t15(i) values(1);
    insert into t15(i) values(2);
    select 'Passed 4 - Insert' from t15 where j = i/i having count(*) = 2;

    update t15 set i = 99 where i = 2;
    select 'Passed 4 - Update' from t15 where j = i/i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+2) */
    /*-------------------*/
    create table t20 (i integer, j computed by (i+2));
    commit; -- t20;
    insert into t20(i) values(1);
    insert into t20(i) values(2);
    select 'Passed 5 - Insert' from t20 where j = i+2 having count(*) = 2;

    update t20 set i = 99 where i = 2;
    select 'Passed 5 - Update' from t20 where j = i+2 having count(*) = 2;

    /*-------------------*/
    /* Computed by (i*2) */
    /*-------------------*/
    create table t25 (i integer, j computed by (i*2));
    commit; -- t25;
    insert into t25(i) values(1);
    insert into t25(i) values(2);
    select 'Passed 6 - Insert' from t25 where j = i*2 having count(*) = 2;

    update t25 set i = 99 where i = 2;
    select 'Passed 6 - Update' from t25 where j = i*2 having count(*) = 2;

    /*-------------------*/
    /* Computed by (i/2) */
    /*-------------------*/
    create table t30 (i integer, j computed by (i/2));
    commit; -- t30;
    insert into t30(i) values(1);
    insert into t30(i) values(2);
    select 'Passed 7 - Insert' from t30 where j = i/2 having count(*) = 2;

    update t30 set i = 99 where i = 2;
    select 'Passed 7 - Update' from t30 where j = i/2 having count(*) = 2;

    /*------------------*/
    /* Computed by (-i) */
    /*------------------*/
    create table t35 (i integer, j computed by (-i));
    commit; -- t35;
    insert into t35(i) values(1);
    insert into t35(i) values(2);
    select 'Passed 8 - Insert' from t35 where j = -i having count(*) = 2;

    update t35 set i = 99 where i = 2;
    select 'Passed 8 - Update' from t35 where j = -i having count(*) = 2;

    /*------------------*/
    /* Computed by (+i) */
    /*------------------*/

    create table t40 (i integer, j computed by (+i));
    commit; -- t40;
    insert into t40(i) values(1);
    insert into t40(i) values(2);
    select 'Passed 9 - Insert' from t40 where j = +i having count(*) = 2;

    update t40 set i = 99 where i = 2;
    select 'Passed 9 - Update' from t40 where j = +i having count(*) = 2;

    /*---------------------------*/
    /* Computed by ((i-i+i)*i/i) */
    /*---------------------------*/
    create table t43 (i integer, j computed by ((i-i+i)*i/i));
    commit; -- t43;
    insert into t43(i) values(3);
    insert into t43(i) values(4);
    select 'Passed 9.1 - Insert' from t43 where j = i having count(*) = 2;

    update t43 set i = 99 where i = 4;
    select 'Passed 9.1 - Update' from t43 where j = i having count(*) = 2;

    /*-----------------*/
    /* Computed by (0) */
    /*-----------------*/
    create table t45 (i integer, j computed by (0));
    commit; -- t45;
    insert into t45(i) values(1);
    insert into t45(i) values(2);
    select 'Passed 10 - Insert' from t45 where j = 0 having count(*) = 2;

    update t45 set i = 99 where i = 2;
    select 'Passed 10 - Update' from t45 where j = 0 having count(*) = 2;

    /*---------------------------*/
    /* Computed by ((4*2-4+4)/2) */
    /*---------------------------*/
    create table t50 (i integer, j computed by ((4*2-4+4)/2));
    commit; -- t50;
    insert into t50(i) values(1);
    insert into t50(i) values(2);
    select 'Passed 11 - Insert' from t50 where j = (4*2-4+4)/2 having count(*) = 2;

    update t50 set i = 99 where i = 2;
    select 'Passed 11 - Update' from t50 where j = (4*2-4+4)/2 having count(*) = 2;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    Passed 1 - Insert
    Passed 1 - Update
    Passed 2 - Insert
    Passed 2 - Update
    Passed 3 - Insert
    Paseed 3 - Update
    Passed 4 - Insert
    Passed 4 - Update
    Passed 5 - Insert
    Passed 5 - Update
    Passed 6 - Insert
    Passed 6 - Update
    Passed 7 - Insert
    Passed 7 - Update
    Passed 8 - Insert
    Passed 8 - Update
    Passed 9 - Insert
    Passed 9 - Update
    Passed 9.1 - Insert
    Passed 9.1 - Update
    Passed 10 - Insert
    Passed 10 - Update
    Passed 11 - Insert
    Passed 11 - Update
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
