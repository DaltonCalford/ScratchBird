#coding:utf-8

"""
ID:          computed-fields-10
FBTEST:      functional.gtcs.computed_fields_10
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_10.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set heading off;

    -- Test verifies COMPUTED-BY field which expression involves GEN_ID() call.

    create generator gen1;
    set generator gen1 to 1000;
    commit; -- show generator gen1;

    /*----------------------------*/
    /* Computed by (a + gen_id()) */
    /*----------------------------*/
    create table t0 (a integer, genid_field computed by (a + gen_id(gen1, 1)));
    commit; -- t0;
    insert into t0(a) values(10);
    insert into t0(a) values(12);
    select * from t0;

    set generator gen1 to 1000;
    select * from t0;

    /*
    **  Since computed fields are evaluated during run-time, the computed
    **  field with gen_id() will be different every-time. So, the following
    **  select will never have a match.
    */
    set generator gen1 to 1000;
    select * from t0 where genid_field = gen_id(gen1, 1);


"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    10 1011
    12 1014

    10 1011
    12 1014
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
