#coding:utf-8

"""
ID:          issue-970
ISSUE:       970
TITLE:       SKIP is off by one
DESCRIPTION:
JIRA:        CORE-611
FBTEST:      bugs.core_0611
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table A (id integer not null);
    commit;
    insert into A (id) values (1);
    insert into A (id) values (2);
    insert into A (id) values (3);
    commit;
    set list on;
    select skip 0 id from a order by id;
    select skip 2 id from a order by id;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    ID                              2
    ID                              3
    ID                              3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

