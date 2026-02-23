#coding:utf-8

"""
ID:          computed-fields-08
FBTEST:      functional.gtcs.computed_fields_08
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_08.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set heading off;
    /*----------------------*/
    /* Computed by UPPER(a) */
    /*----------------------*/
    create table t0 (a char(25), upper_a computed by (upper(a)));
    commit; -- t0;
    insert into t0(a) values('abcdef');
    insert into t0(a) values('ABCDEF');
    insert into t0(a) values('123456');
    insert into t0(a) values('aBcDeF');
    select 'Passed 1 - Insert' from t0 where upper_a = upper(a) having count(*) = 4;

    update t0 set a = 'xyz' where a = 'abc';
    select 'Passed 1 - Update' from t0 where upper_a = upper(a) having count(*) = 4;

    /*-----------------------------------*/
    /* Computed by a || UPPER('upper()') */
    /*-----------------------------------*/
    create table t5 (a char(25), upper_const computed by (a || upper('upper()')));
    commit; -- t5;
    insert into t5(a) values('abcdef');
    insert into t5(a) values('ABCDEF');
    insert into t5(a) values('123456');
    insert into t5(a) values('aBcDeF');
    select 'Passed 2 - Insert' from t5 where upper_const = a || upper('upper()') having count(*) = 4;

    update t5 set a = 'xyz' where a = 'abcdef';
    select 'Passed 2 - Update' from t5 where upper_const = a || upper('upper()') having count(*) = 4;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    Passed 1 - Insert
    Passed 1 - Update
    Passed 2 - Insert
    Passed 2 - Update
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
