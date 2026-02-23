#coding:utf-8

"""
ID:          computed-fields-02
FBTEST:      functional.gtcs.computed_fields_02
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_02.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    /*
    ** Syntax test cases - Valid Arithmetic operations on
    ** SMALLINT, NUMERIC, DECIMAL, FLOAT, DOUBLE PRECISION
    */

    /* SMALLINT: */
    /*-----------------*/
    /* Computed by (i) */
    /*-----------------*/
    create table t0_s  (i smallint, j computed by (i));
    commit; -- t0_s;
    insert into t0_s(i) values(-32768);
    insert into t0_s(i) values(32767);
    select 'Passed 1(s) - Insert' from t0_s where j = i having count(*) = 2;

    update t0_s set i = -32768 where i = 2;
    select 'Passed 1(s) - Update' from t0_s where j = i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+i) */
    /*-------------------*/
    create table t5_s  (i smallint, j computed by (i+i));
    commit; -- t5_s;
    insert into t5_s(i) values(-32768);
    insert into t5_s(i) values(32767);
    select 'Passed 2(s) - Insert' from t5_s where j = i+i having count(*) = 2;

    update t5_s set i = -32768 where i = 32767;
    select 'Passed 2(s) - Update' from t5_s where j = i+i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+32768) */
    /*-------------------*/
    create table t10_s (i smallint, j computed by (i+32768));
    commit; -- t10_s;
    insert into t10_s(i) values(-32768);
    insert into t10_s(i) values(32767);
    select 'Passed 3(s) - Insert' from t10_s where j = i+32768 having count(*) = 2;

    update t10_s set i = -32768 where i = 32767;
    select 'Passed 3(s) - Update' from t10_s where j = i+32768 having count(*) = 2;


    /* DECIMAL: */
    /*-----------------*/
    /* Computed by (i) */
    /*-----------------*/
    create table t0_d  (i decimal(15,2), j computed by (i));
    commit; -- t0_d;
    insert into t0_d(i) values(1);
    insert into t0_d(i) values(2);
    select 'Passed 1(d) - Insert' from t0_d where j = i having count(*) = 2;

    update t0_d set i = 99 where i = 2;
    select 'Passed 1(d) - Update' from t0_d where j = i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+i) */
    /*-------------------*/
    create table t5_d  (i decimal(15,2), j computed by (i+i));
    commit; -- t5_d;
    insert into t5_d(i) values(1.0);
    insert into t5_d(i) values(2.0);
    select 'Passed 2(d) - Insert' from t5_d where j = i+i having count(*) = 2;

    update t5_d set i = 99.0 where i = 2.0;
    select 'Passed 2(d) - Update' from t5_d where j = i+i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+999999999999.99) */
    /*-------------------*/
    create table t10_d (i decimal(15,2), j computed by (i+999999999999.99));
    commit; -- t10_d;
    insert into t10_d(i) values(0.01);
    insert into t10_d(i) values(0.02);
    select 'Passed 3(d) - Insert' from t10_d where j = i+999999999999.99 having count(*) = 2;

    update t10_d set i = 0.2 where i = 0.02;
    select 'Passed 3(d) - Update' from t10_d where j = i+999999999999.99 having count(*) = 2;

    -- ##########################################################################

    /* NUMERIC: */
    /*-----------------*/
    /* Computed by (i) */
    /*-----------------*/
    create table t0_n  (i numeric(15,2), j computed by (i));
    commit; -- t0_n;
    insert into t0_n(i) values(1.0);
    insert into t0_n(i) values(2.0);
    select 'Passed 1(n) - Insert' from t0_n where j = i having count(*) = 2;

    update t0_n set i = 99.0 where i = 2.0;
    select 'Passed 1(n) - Update' from t0_n where j = i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+i) */
    /*-------------------*/
    create table t5_n  (i numeric(15,2), j computed by (i+i));
    commit; -- t5_n;
    insert into t5_n(i) values(1.0);
    insert into t5_n(i) values(2.0);
    select 'Passed 2(n) - Insert' from t5_n where j = i+i having count(*) = 2;

    update t5_n set i = 99.0 where i = 2.0;
    select 'Passed 2(n) - Update' from t5_n where j = i+i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+999999999999.99) */
    /*-------------------*/
    create table t10_n (i decimal(15,2), j computed by (i+999999999999.99));
    commit; -- t10_n;
    insert into t10_n(i) values(0.01);
    insert into t10_n(i) values(0.02);
    select 'Passed 3(n) - Insert' from t10_n where j = i+999999999999.99 having count(*) = 2;

    update t10_n set i = 0.2 where i = 0.02;
    select 'Passed 3(n) - Update' from t10_n where j = i+999999999999.99 having count(*) = 2;

    -- ##########################################################################

    /* FLOAT: */
    -- https://en.wikipedia.org/wiki/Single-precision_floating-point_format
    -- IEEE 754 32-bit base-2 floating-point variable has a maximum value of (2-2^23)*2^127 ==> ~3.4028234663852886e+38
    -- All integers with 7 or fewer decimal digits, and any 2^n for a whole number -149<=n<=127, can be converted exactly
    -- into an IEEE 754 single-precision floating-point value.

    /*-----------------*/
    /* Computed by (i) */
    /*-----------------*/
    create table t0_f  (i float, j computed by (i));
    commit; -- t0_f;
    insert into t0_f(i) values(1.0);
    insert into t0_f(i) values(2.0);
    select 'Passed 1(f) - Insert' from t0_f where j = i having count(*) = 2;

    update t0_f set i = 99.0 where i = 2.0;
    select 'Passed 1(f) - Update' from t0_f where j = i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+i) */
    /*-------------------*/
    create table t5_f  (i float, j computed by (i+i));
    commit; -- t5_f;
    insert into t5_f(i) values(1.0);
    insert into t5_f(i) values(2.0);
    select 'Passed 2(f) - Insert' from t5_f where j = i+i having count(*) = 2;

    update t5_f set i = 99.0 where i = 2.0;
    select 'Passed 2(f) - Update' from t5_f where j = i+i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+1) */
    /*-------------------*/
    create table t10_f (i float, j computed by (i+1));
    commit; -- t10_f;
    insert into t10_f(i) values(1.0);
    insert into t10_f(i) values(2.0);
    select 'Passed 3(f) - Insert' from t10_f where j = i+1 having count(*) = 2;

    update t10_f set i = 99.0 where i = 2.0;
    select 'Passed 3(f) - Update' from t10_f where j = i+1 having count(*) = 2;

    -- ##########################################################################

    /* DOUBLE PRECISION: */
    /*-----------------*/
    /* Computed by (i) */
    /*-----------------*/
    create table t0_dp  (i double precision, j computed by (i));
    commit; -- t0_dp;
    insert into t0_dp(i) values(1.0);
    insert into t0_dp(i) values(2.0);
    select 'Passed 1(dp) - Insert' from t0_dp where j = i having count(*) = 2;

    update t0_dp set i = 0.2 where i = 2.0;
    select 'Passed 1(dp) - Update' from t0_dp where j = i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+i) */
    /*-------------------*/
    create table t5_dp  (i double precision, j computed by (i+i));
    commit; -- t5_dp;
    insert into t5_dp(i) values(1.0);
    insert into t5_dp(i) values(2.0);
    select 'Passed 2(dp) - Insert' from t5_dp where j = i+i having count(*) = 2;

    update t5_dp set i = 0.2 where i = 2.0;
    select 'Passed 2(dp) - Update' from t5_dp where j = i+i having count(*) = 2;

    /*-------------------*/
    /* Computed by (i+1) */
    /*-------------------*/
    create table t10_dp (i double precision, j computed by (i+1));
    commit; -- t10_dp;
    insert into t10_dp(i) values(1.0);
    insert into t10_dp(i) values(2.0);
    select 'Passed 3(dp) - Insert' from t10_dp where j = i+1 having count(*) = 2;

    update t10_dp set i = 0.2 where i = 2.0;
    select 'Passed 3(dp) - Update' from t10_dp where j = i+1 having count(*) = 2;

"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    Passed 1(s) - Insert
    Passed 1(s) - Update
    Passed 2(s) - Insert
    Passed 2(s) - Update
    Passed 3(s) - Insert
    Passed 3(s) - Update

    Passed 1(d) - Insert
    Passed 1(d) - Update
    Passed 2(d) - Insert
    Passed 2(d) - Update
    Passed 3(d) - Insert
    Passed 3(d) - Update

    Passed 1(n) - Insert
    Passed 1(n) - Update
    Passed 2(n) - Insert
    Passed 2(n) - Update
    Passed 3(n) - Insert
    Passed 3(n) - Update

    Passed 1(f) - Insert
    Passed 1(f) - Update
    Passed 2(f) - Insert
    Passed 2(f) - Update
    Passed 3(f) - Insert
    Passed 3(f) - Update

    Passed 1(dp) - Insert
    Passed 1(dp) - Update
    Passed 2(dp) - Insert
    Passed 2(dp) - Update
    Passed 3(dp) - Insert
    Passed 3(dp) - Update
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
