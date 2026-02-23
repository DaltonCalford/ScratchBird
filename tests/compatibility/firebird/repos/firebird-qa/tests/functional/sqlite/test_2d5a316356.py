#coding:utf-8

"""
ID:          2d5a316356
ISSUE:       https://www.sqlite.org/src/tktview/2d5a316356
TITLE:       Segmentation fault in CROSS JOIN
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table rt0(c0 int, c1 int, c2 int);
    create view v0(c0) as select 1 from rdb$database;
    insert into rt0(c1) values (1);
    set count on;
    select v0.c0, rt0.c0, rt0.c1, rt0.c2
    from v0
    cross join rt0
    where
        rt0.c1 in (select 1 from rdb$database)
        and rt0.c1 > 0;

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 1
    C0 <null>
    C1 1
    C2 <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
