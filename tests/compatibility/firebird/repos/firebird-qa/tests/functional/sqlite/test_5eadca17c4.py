#coding:utf-8

"""
ID:          5eadca17c4
ISSUE:       https://www.sqlite.org/src/tktview/5eadca17c4
TITLE:       Assertion
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test_a(c0 int);
    create table test_b (c0 int, c1 int, c2 int);
    create view v_test as select 0 as c0 from rdb$database rows 0;

    insert into test_a values (0);
    insert into test_a values (1);
    insert into test_b(c0) values (0);
    insert into test_b(c0) values (1);

    set count on;
    select * from test_a left join test_b using(c0) inner join v_test using(c0);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
