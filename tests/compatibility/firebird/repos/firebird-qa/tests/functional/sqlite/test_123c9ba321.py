#coding:utf-8

"""
ID:          123c9ba321
ISSUE:       https://www.sqlite.org/src/tktview/123c9ba321
TITLE:       Incorrect result when an index is used for an ordered join
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
    create table t1(a int, b int);
    create table t2(x int, y int);
    insert into t1 values(1,2);
    insert into t2 values(1,3);

    set count on;
    select y from t1, t2 where a=x and b<=y order by b desc;
    commit;
    create index t1ab on t1(a,b);
    set plan on;
    select y from t1, t2 where a=x and b<=y order by b desc;
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
    Y 3
    Records affected: 1

    PLAN SORT (JOIN (T2 NATURAL, T1 INDEX (T1AB)))
    Y 3
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
