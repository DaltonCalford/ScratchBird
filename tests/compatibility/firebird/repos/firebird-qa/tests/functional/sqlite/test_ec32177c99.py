#coding:utf-8

"""
ID:          ec32177c99
ISSUE:       https://www.sqlite.org/src/tktview/ec32177c99
TITLE:       Incorrect result with complex OR-connected WHERE
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Execution plan (in legacy form) contains excessive comma on FB 6.x (regression),
    see: https://github.com/FirebirdSQL/firebird/issues/8711
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824

    [21.08.2025] pzotov
    Added 'set plan on' because GH-8711 has been fixed. Checked on 6.0.0.1232.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a integer primary key using index t1_a, b varchar(10));
    insert into t1(a,b) values(1,1);
    insert into t1(a,b) values(2,null);
    insert into t1(a,b) values(3,null);
    commit;

    create view v_test as
    select a
    from t1 x
    where 2 > (
        select count(*) from t1 y
        where
            x.b is not null and y.b is null
            or y.b < x.b
            or x.b is not distinct from y.b and y.a > x.a
    );

    set count on;
    select * from v_test;
    create index t1_b on t1(b);
    set plan on;
    select * from v_test;
"""

substitutions = [('[ \t]+', ' ')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 2
    A 3
    Records affected: 2

    PLAN (V_TEST Y INDEX (T1_B, T1_B, T1_A, T1_B))
    PLAN (V_TEST X NATURAL)
    A 2
    A 3
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
