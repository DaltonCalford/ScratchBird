#coding:utf-8

"""
ID:          computed-fields-06
FBTEST:      functional.gtcs.computed_fields_06
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

    /*
    ** Test case which defines computed field with expression that involves more than one column
    */

    /*-------------------*/
    /* Computed by (a*b) */
    /*-------------------*/
    create table t0 (a integer, b integer, a_b computed by (a*b));
    commit; -- t0;
    insert into t0(a,b) values(10,10);
    insert into t0(a,b) values(11,11);
    select 'Passed 1 - Insert' from t0 where a_b = a*b having count(*) = 2;

    update t0 set a = 12, b = 12 where a = 10;
    update t0 set a = 13         where a = 11;
    select 'Passed 1 - Update' from t0 where a_b = a*b having count(*) = 2;

    /*---------------------*/
    /* Computed by (a*b/c) */
    /*---------------------*/
    create table t5 (a integer, b integer, c integer, a_b_c computed by (a*b/c));
    commit; -- t5;
    insert into t5(a,b,c) values(10,10,10);
    insert into t5(a,b,c) values(11,11,11);
    select 'Passed 2 - Insert' from t5 where a_b_c = a*b/c having count(*) = 2;

    update t5 set a = 12, b = 12, c = 12 where a = 10;
    update t5 set a = 13                 where a = 11;
    select 'Passed 2 - Update' from t5 where a_b_c = a*b/c having count(*) = 2;

    /*----------------------*/
    /* Computed by (a/10*b) */
    /*----------------------*/
    create table t10 (a integer, b integer, a_b_const computed by (a/10*b));
    commit; -- t10;
    insert into t10(a,b) values(10,10);
    insert into t10(a,b) values(11,11);
    select 'Passed 3 - Insert' from t10 where a_b_const = a/10*b having count(*) = 2;

    update t10 set a = 12, b = 12 where a = 10;
    update t10 set a = 13         where a = 11;
    select 'Passed 3 - Update' from t10 where a_b_const = a/10*b having count(*) = 2;

"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    Passed 1 - Insert
    Passed 1 - Update
    Passed 2 - Insert
    Passed 2 - Update
    Passed 3 - Insert
    Passed 3 - Update
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
