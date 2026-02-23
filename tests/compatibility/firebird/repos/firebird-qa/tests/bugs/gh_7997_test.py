#coding:utf-8

"""
ID:          issue-7997
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7997
TITLE:       Unexpected results when comparing integer with string containing value out of range of that integer datatype
NOTES:
    [11.03.2024] pzotov.
    Confirmed problem in 6.0.0.274: some expressions fail with "SQLSTATE = 22003 / ... / -numeric value is out of range".
    Checked 6.0.0.276 -- all fine.
    Checked 5.0.1.1358 (25a643a).
    Checked 4.0.5.3066 (9930033).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table t_sml(x smallint, primary key(x) using index sml_pk); -- pk is needed
    recreate table t_int(x integer, primary key(x) using index int_pk); -- pk is needed
    recreate table t_bigint(x bigint, primary key(x) using index bigint_pk); -- pk is needed
    recreate table t_int128(x int128, primary key(x) using index int128_pk); -- pk is needed

    insert into t_sml(x)    values (-1);
    insert into t_int(x)    values (-1);
    insert into t_bigint(x) values (-1);
    insert into t_int128(x) values (-1);

    set count on;

    -- ########################################## check-1 ####################################################

    select t.x as sml_r6 from t_sml t       where t.x = -1 and t.x <= ( (-170141183460469231731687303715884105728) || 1 );
    select t.x as int_r6 from t_int t       where t.x = -1 and t.x <= ( (-170141183460469231731687303715884105728) || 1 );
    select t.x as bigint_r6 from t_bigint t where t.x = -1 and t.x <= ( (-170141183460469231731687303715884105728) || 1 );
    select t.x as int128_r6 from t_int128 t where t.x = -1 and t.x <= ( (-170141183460469231731687303715884105728) || 1 );
    select t.x as sml_r6 from t_sml t       where  t.x <= ( (-170141183460469231731687303715884105728) || 1 );
    select t.x as int_r6 from t_int t       where  t.x <= ( (-170141183460469231731687303715884105728) || 1 );
    select t.x as bigint_r6 from t_bigint t where  t.x <= ( (-170141183460469231731687303715884105728) || 1 );
    select t.x as int128_r6 from t_int128 t where  t.x <= ( (-170141183460469231731687303715884105728) || 1 );
    set count off;

    delete from t_sml;
    delete from t_int;
    delete from t_bigint;
    delete from t_int128;

    insert into t_sml(x)    values (1);
    insert into t_int(x)    values (1);
    insert into t_bigint(x) values (1);
    insert into t_int128(x) values (1);

    -- ########################################## check-2 ####################################################
    set count on;
    select t.x as sml_r6 from t_sml t       where  t.x = 1 and t.x >= ( (170141183460469231731687303715884105727) || 1 );
    select t.x as int_r6 from t_int t       where  t.x = 1 and t.x >= ( (170141183460469231731687303715884105727) || 1 );
    select t.x as bigint_r6 from t_bigint t where  t.x = 1 and t.x >= ( (170141183460469231731687303715884105727) || 1 );
    select t.x as int128_r6 from t_int128 t where  t.x = 1 and t.x >= ( (170141183460469231731687303715884105727) || 1 );
    select t.x as sml_r6 from t_sml t       where  t.x >= ( (170141183460469231731687303715884105727) || 1 );
    select t.x as int_r6 from t_int t       where  t.x >= ( (170141183460469231731687303715884105727) || 1 );
    select t.x as bigint_r6 from t_bigint t where  t.x >= ( (170141183460469231731687303715884105727) || 1 );
    select t.x as int128_r6 from t_int128 t where  t.x >= ( (170141183460469231731687303715884105727) || 1 );
"""

act = isql_act('db', test_script, substitutions = [('[ \t]+', ' ')])

expected_stdout = """
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
"""

@pytest.mark.version('>=4.0.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
