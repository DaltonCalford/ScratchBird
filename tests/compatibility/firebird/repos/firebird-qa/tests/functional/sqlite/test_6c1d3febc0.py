#coding:utf-8

"""
ID:          6c1d3febc0
ISSUE:       https://www.sqlite.org/src/tktview/6c1d3febc0
TITLE:       PRIMARY KEY for REAL column datatype leads to a missing entry in the index.
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1 (c0 real, c1 real, unique(c0, c1) using index t1_unq);
    insert into t1(c0, c1) values (0, 9223372036854775807);
    insert into t1(c0, c1) values (0, 0);
    update t1 set c0 = null;

    set plan on;
    set count on;
    select *  from t1 where c0 is null;

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
    PLAN (T1 INDEX (T1_UNQ))
    
    C0 <null>
    C1 9.223372e+18

    C0 <null>
    C1 0

    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
