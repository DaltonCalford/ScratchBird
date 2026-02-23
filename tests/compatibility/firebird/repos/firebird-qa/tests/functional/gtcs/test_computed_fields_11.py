#coding:utf-8

"""
ID:          computed-fields-11
FBTEST:      functional.gtcs.computed_fields_11
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_11.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

substitutions = [  ('^((?!Statement failed|SQL error code|Column unknown|F01|F02|REL_NAME|Records).)*$', '')
                  ,('(")?F01(")?', 'F01')
                  ,('(")?F02(")?', 'F02')
                  ,('[ \t]+', ' ')
                ]

db = db_factory()

test_script = """
    set heading off;
    set list on;
    set count on;

    /*-----------------------------------------*/
    /* Computed field using non-existing field */
    /*-----------------------------------------*/
    create table t0 (f01 integer, f_calc computed by (f02));

    /*--------------------------------------------*/
    /* Computed field using not yet defined field */
    /*--------------------------------------------*/
    create table t1 (f_calc computed by (f01), f01 integer);

    recreate table t2(f01 int, f99 int);
    commit;
    alter table t2
        add f_calc computed by( f01 + f99)
        ,alter f99 position 3 -- MUST PASS! FB 3+ will produce correct script if we want to extract metadata using isql -x.
        --,drop f99 -- Statement failed, SQLSTATE = 42S22 / invalid request BLR at offset 13 / -column f99 is not defined in table T2
    ;
    commit;

    select r.rdb$relation_name as rel_name
    from rdb$relations r
    where r.rdb$relation_name in ( upper('t0'), upper('t1'), upper('t2')  )
    ;
"""

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    REL_NAME T2
    Records affected: 1
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42S22
    unsuccessful metadata update
    -CREATE TABLE T0 failed
    -Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -F02

    Statement failed, SQLSTATE = 42S22
    unsuccessful metadata update
    -CREATE TABLE T1 failed
    -Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -F01
"""

@pytest.mark.version('>=2.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
