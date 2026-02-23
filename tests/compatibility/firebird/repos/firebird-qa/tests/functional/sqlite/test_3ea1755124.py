#coding:utf-8

"""
ID:          3ea1755124
ISSUE:       https://www.sqlite.org/src/tktview/3ea1755124
TITLE:       REINDEX causes "UNIQUE constraint failed" error for generated column
DESCRIPTION:
NOTES:
    [17.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    create table t0(c0 int, c1 int generated always as identity unique using index t0_c1_unq);
    insert into t0(c0) values (1);
    commit;
    alter index t0_c1_unq active;
    insert into t0(c0) values (0);
    commit;
    alter index t0_c1_unq active;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 1
    Records affected: 1
"""

@pytest.mark.version('>=4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
