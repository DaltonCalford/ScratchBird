#coding:utf-8

"""
ID:          5c6955204c
ISSUE:       https://www.sqlite.org/src/tktview/5c6955204c
TITLE:       Incorrect result on a table scan of a partial index
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 boolean);
    create index index_0 on t0(c0) where c0 is not null;
    insert into t0(c0) values(null);
    set count on;
    select * from t0 where c0 is not null or not false;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 <null>
    Records affected: 1
"""

@pytest.mark.version('>=5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
