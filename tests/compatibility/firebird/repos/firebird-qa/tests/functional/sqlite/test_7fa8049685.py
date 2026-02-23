#coding:utf-8

"""
ID:          7fa8049685
ISSUE:       https://www.sqlite.org/src/tktview/7fa8049685
TITLE:       Incorrect result on a LEFT JOIN when using an index
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x char(3));
    insert into t1 values(1);
    create table t2(y char(3), z char(3));
    create view v_test as  select coalesce(z, '!!!') as txt from t1 left join t2 on ( x = y || coalesce(z, '!!!'));

    set count on;
    set plan on;
    select * from v_test;
    commit;
    create index t2i on t2 computed by(y || coalesce(z, '!!!'));
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
    PLAN JOIN (V_TEST T1 NATURAL, V_TEST T2 NATURAL)
    TXT !!!
    Records affected: 1

    PLAN JOIN (V_TEST T1 NATURAL, V_TEST T2 INDEX (T2I))
    TXT !!!
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
