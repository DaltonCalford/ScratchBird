#coding:utf-8

"""
ID:          issue-4323
ISSUE:       4323
TITLE:       "attempted update of read-only column" when trying update editable view without triggers
DESCRIPTION:
JIRA:        CORE-3991
FBTEST:      bugs.core_3991
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter view v_test as select 1 id from rdb$database;
    commit;
    recreate table test_table (id int);
    create or alter view v_test as select id from test_table;
    commit;

    insert into v_test(id) values(10);
    commit;

    set count on;
    update v_test set id = 10;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

