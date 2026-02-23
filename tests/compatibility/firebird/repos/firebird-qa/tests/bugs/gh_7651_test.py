#coding:utf-8

"""
ID:          issue-7651
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7651
TITLE:       Unable to find savepoint in insert with nested query and returning clause in FB4
DESCRIPTION:
NOTES:
    [27.06.2023] pzotov
    Confirmed bug on 4.0.3.2955.
    Checked on 3.0.11.33695, 4.0.3.2956 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set bail on;
    recreate table test (
        id int primary key using index test_id
       ,val int not null
    );

    savepoint s;
    insert into test(id, val) values (1, (select count(*) From test Where id = 1)) returning id, val;
    release savepoint s; -- this caused "SQLSTATE = 3B000 / Unable to find savepoint with name S in transaction context"
    select 'Completed' as msg from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    VAL                             0
    MSG                             Completed
"""

@pytest.mark.version('>=3.0.11')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
