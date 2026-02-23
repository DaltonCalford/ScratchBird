#coding:utf-8

"""
ID:          2ea3e9fe63
ISSUE:       https://www.sqlite.org/src/tktview/2ea3e9fe63
TITLE:       Partial index causes assertion fault on UPDATE OR REPLACE
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
    create table t1(a int primary key, b int);
    create unique index t1ab on t1(a,b);
    create index t1b on t1(b) where b=1;

    insert into t1(a,b) values(123,456);
    update or insert into t1(a,b) values(123,789) matching(a);
    update or insert into t1(a,b) values(-99,789) matching(b);
    set count on;
    select * from t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A -99
    B 789
    Records affected: 1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
