#coding:utf-8

"""
ID:          computed-fields-14
FBTEST:      functional.gtcs.computed_fields_14
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_14.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    /*---------------------------------------------*/
    /* Create a table with computed field.         */
    /*---------------------------------------------*/
    create table t0 (a integer, af computed by (a*3));
    insert into t0(a) values(10);

    /*---------------------------------------------------------------*/
    /* Insert a value into computed-field column, which should fail. */
    /*---------------------------------------------------------------*/
    insert into t0(af) values(11);
    select 'point-1' msg, p.* from t0 p;

    /*---------------------------------------------------------------*/
    /* Update the computed-field column directly, which should fail. */
    /*---------------------------------------------------------------*/
    update t0 set af = 99 where a = 10;
    select 'point-2' msg, p.* from t0 p;

    /*---------------------------------------------------------------*/
    /* Create a table with only a computed-field should fail.        */
    /*---------------------------------------------------------------*/
    create table t5 (af computed by (1+2));

    /*-----------------------------------------------------------------*/
    /* Create a table with a computed-field, which has constant value. */
    /* Trying to insert a value in it should fail.                     */
    /*-----------------------------------------------------------------*/
    create table t6 (af int, bf computed by (1+2));
    insert into t6 values(10, 12);
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' '),
                                                 ('attempted update of read-only column.*',
                                                  'attempted update of read-only column')])

@pytest.mark.version('>=3')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_TABLE_NAME = 'T5' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"T5"'
    expected_stdout = f"""
        Statement failed, SQLSTATE 42000
        attempted update of read-only column
        point-1 10 30
        Statement failed, SQLSTATE 42000
        attempted update of read-only column
        point-2 10 30
        Statement failed, SQLSTATE 42000
        unsuccessful metadata update
        -TABLE {TEST_TABLE_NAME}
        -Can't have relation with only computed fields or constraints
        Statement failed, SQLSTATE 21S01
        Dynamic SQL Error
        -SQL error code -804
        -Count of read-write columns does not equal count of values
    """    

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
