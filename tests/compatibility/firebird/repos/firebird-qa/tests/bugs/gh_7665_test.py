#coding:utf-8

"""
ID:          issue-7665
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7665
TITLE:       Wrong result ordering in LEFT JOIN query
DESCRIPTION:
NOTES:
    Confirmed bug on 3.0.11.33691, 4.0.3.2957 (FB 5.x seems not affected).
    Checked on 3.0.11.33695, 4.0.3.2966, 5.0.0.1121: all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = f"""
    set list on;
    create table tmain (x int primary key);
    create table tdetl (x int primary key, y int references tmain);
    commit;

    insert into tmain values (1);
    insert into tmain values (2);
    insert into tmain values (3);
    insert into tmain values (4);
    insert into tdetl values (1, 1);
    insert into tdetl values (2, 1);
    insert into tdetl values (3, 4);
    insert into tdetl values (4, 2);

    select a.x, b.x
    from tdetl a
    left outer join tmain b
      on
         a.y = b.x
         and a.x = b.x
    order by a.x
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               1
    X                               1

    X                               2
    X                               <null>

    X                               3
    X                               <null>

    X                               4
    X                               <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
