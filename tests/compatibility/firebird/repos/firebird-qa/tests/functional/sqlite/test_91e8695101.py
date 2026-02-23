#coding:utf-8

"""
ID:          91e8695101
ISSUE:       https://www.sqlite.org/src/tktview/91e8695101
TITLE:       Segfault in a table with generated columns
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    --set echo on;
    create table t0(id int generated always as identity primary key, c1 int generated always as identity unique, c2 int unique);

    insert into t0(id) overriding system value values (null) returning *;
    insert into t0(id, c1) overriding system value values (-1, null) returning *;
    insert into t0(id, c1, c2) overriding system value values (-2, -3, -4) returning *;
    insert into t0 default values returning *;
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
    Statement failed, SQLSTATE = 23000
    validation error for column "T0"."ID", value "*** null ***"

    Statement failed, SQLSTATE = 23000
    validation error for column "T0"."C1", value "*** null ***"

    ID -2
    C1 -3
    C2 -4

    ID 1
    C1 2
    C2 <null>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
