#coding:utf-8

"""
ID:          2326c258d0
ISSUE:       https://www.sqlite.org/src/tktview/2326c258d0
TITLE:       Incorrect result when a LEFT JOIN provides the qualifying constraint for a partial index
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int);
    create table t2(b int);

    insert into t1 values(1);
    commit;

    create index t1x on t1(a) where a = 1;
    set count on;
    set plan on;
    select * from t1 left join t2 on t1.a = t2.b where t1.a = 1 order by t1.a;

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
    PLAN JOIN (T1 ORDER T1X, T2 NATURAL)
    A 1
    B <null>
    Records affected: 1
"""

@pytest.mark.version('>=5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
