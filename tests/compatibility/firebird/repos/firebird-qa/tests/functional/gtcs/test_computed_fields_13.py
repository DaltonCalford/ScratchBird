#coding:utf-8

"""
ID:          computed-fields-13
FBTEST:      functional.gtcs.computed_fields_13
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_13.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script

  Check that it is not allowed to drop column which is referenced by computed-by column.
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

    /*---------------------------------------------*/
    /* Create a table with nested computed field.  */
    /*---------------------------------------------*/
    create table t1 (a integer, af computed by (a*4), afaf computed by (af*5));
    insert into t1(a) values(11);

    commit;

    /*---------------------------------------------------------------------*/
    /* Now alter table and drop the field which is used in computed field. */
    /* It shouldn't allow you to drop the field.                           */
    /*---------------------------------------------------------------------*/
    alter table t0 drop a;
    select 'point-1' msg, p.* from t0 p;

    /*---------------------------------------------------------------------*/
    /* Now alter table and drop the computed field which is used in other  */
    /* computed field.                                                     */
    /* It shouldn't allow you to drop the field.                           */
    /*---------------------------------------------------------------------*/
    alter table t1 drop af;
    select 'point-2' msg, p.* from t1 p;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])


@pytest.mark.version('>=3')
def test_1(act: Action):

    expected_stdout_5x = """
        Statement failed, SQLSTATE 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN T0.A
        -there are 1 dependencies
        point-1 10 30
        Statement failed, SQLSTATE 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN T1.AF
        -there are 1 dependencies
        point-2 11 44 220
    """
    expected_stdout_6x = """
        Statement failed, SQLSTATE 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN "PUBLIC"."T0"."A"
        -there are 1 dependencies
        point-1 10 30
        Statement failed, SQLSTATE 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN "PUBLIC"."T1"."AF"
        -there are 1 dependencies
        point-2 11 44 220
    """
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
