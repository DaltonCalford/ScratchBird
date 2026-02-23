#coding:utf-8

"""
ID:          issue-969
ISSUE:       969
TITLE:       FIRST is applied before aggregation
DESCRIPTION:
JIRA:        CORE-610
FBTEST:      bugs.core_0610
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table A (id integer not null);
    create table B (id integer not null, A integer not null, v integer);
    commit;
    insert into A (id) values (1);
    insert into A (id) values (2);
    insert into A (id) values (3);
    insert into B (id, A, v) values (1, 1, 1);
    insert into B (id, A, v) values (2, 1, 1);
    insert into B (id, A, v) values (3, 2, 2);
    insert into B (id, A, v) values (4, 2, 2);
    insert into B (id, A, v) values (5, 3, 3);
    insert into B (id, A, v) values (6, 3, 3);
    commit;
    set list on;
    select first 1 count(*) from a;
    select first 2 a.id, sum(b.v) from A,B where a.id = b.a
    group by a.id
    order by a.id;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    COUNT                           3
    ID                              1
    SUM                             2
    ID                              2
    SUM                             4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

