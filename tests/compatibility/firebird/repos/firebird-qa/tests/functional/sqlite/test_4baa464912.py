#coding:utf-8

"""
ID:          4baa464912
ISSUE:       https://www.sqlite.org/src/tktview/4baa464912
TITLE:       NULL handling for indexes on expressions
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int);
    insert into t1 values(null);
    insert into t1 values(1);
    select '1' msg, a from t1 where a < 10;
    select '2' msg, a from t1 where a+0 < 10;
    commit;

    create index t1x1 on t1(a);
    create index t1x2 on t1 computed by (a+0);

    set count on;
    set plan on;
    select '3' msg, a from t1 where a < 10;
    select '4' msg, a from t1 where a+0 < 10;
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
    MSG 1
    A 1
    MSG 2
    A 1

    PLAN (T1 INDEX (T1X1))
    MSG 3
    A 1
    Records affected: 1

    PLAN (T1 INDEX (T1X2))
    MSG 4
    A 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
