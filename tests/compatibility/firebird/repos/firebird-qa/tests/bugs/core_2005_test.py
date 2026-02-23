#coding:utf-8

"""
ID:          issue-2442
ISSUE:       2442
TITLE:       Support SQL 2008 syntax for MERGE statement with DELETE extension
DESCRIPTION:
JIRA:        CORE-2005
FBTEST:      bugs.core_2005
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table src(id int primary key, x int, y int, z computed by(x+y));
    recreate table tgt(id int primary key, x int, y int, z computed by(x+y));
    commit;

    insert into src values(1, 5, 2);
    insert into src values(3, 4, 3);
    insert into src values(5, 3, 4);
    insert into src values(6, 2, 5);
    insert into src values(7, 1, 1);
    insert into src values(8, 0, 0);
    insert into src values(9, 1, 2);
    commit;


    insert into tgt values(2, 2, 5);
    insert into tgt values(3, 3, 5);
    insert into tgt values(4, 1, 7);
    insert into tgt values(5, 6, 3);
    commit;

    set transaction snapshot no wait;

    set term ^;
    execute block as
    begin
        in autonomous transaction  do
        merge into tgt t
        using src s
        on s.id = t.id
        when not matched and s.z >= 7 then
            insert values(id, x, y)
        when matched and t.z > s.z then
            delete
        when matched then
            update set x = s.x, y = s.y
        ;

        merge into src t
        using tgt s
        on s.id = t.id
        when matched and t.z <= s.z then
            update set x = s.x, y = s.y
        when not matched and s.z >= 7 then
            insert values(id, x, y)
        when matched then
            delete
        ;
    end
    ^
    set term ;^
    commit;

    select * from src;
    select * from tgt;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
          ID            X            Y                     Z
============ ============ ============ =====================
           1            5            2                     7
           3            3            5                     8
           5            6            3                     9
           6            2            5                     7
           7            1            1                     2
           8            0            0                     0
           9            1            2                     3
           2            2            5                     7
           4            1            7                     8


          ID            X            Y                     Z
============ ============ ============ =====================
           2            2            5                     7
           4            1            7                     8
           1            5            2                     7
           6            2            5                     7
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

