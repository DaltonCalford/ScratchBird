#coding:utf-8

"""
ID:          issue-5304
ISSUE:       5304
TITLE:       Server crashes during GC when DELETE is executed after adding new referencing column
DESCRIPTION:
JIRA:        CORE-5016
FBTEST:      bugs.core_5016
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table a (x integer primary key);
    create table b (x integer primary key);
    insert into b values (1);
    commit;
    alter table b add y integer references a(x);
    commit;
    delete from b;
    commit;
    set list on;
    select count(*) as k from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    K                               1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

