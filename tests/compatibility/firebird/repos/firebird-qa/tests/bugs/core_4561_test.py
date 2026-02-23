#coding:utf-8

"""
ID:          issue-4878
ISSUE:       4878
TITLE:       BUGCHECK(183) when use cursor with "order by ID+0" and "for update with lock"
DESCRIPTION:
JIRA:        CORE-4561
FBTEST:      bugs.core_4561
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    --  Confirmed on WI-T3.0.0.31852 (CS/SC/SS):
    --  Statement failed, SQLSTATE = XX000
    --  internal Firebird consistency check (wrong record length (183), file: vio.cpp line: 1310)
    --  Statement failed, SQLSTATE = XX000
    --  internal Firebird consistency check (can't continue after bugcheck)
    --  . . .
    recreate table tm(id int);
    commit;
    insert into tm(id) values(1);
    insert into tm(id) values(2); -- at least TWO records need to be added
    commit;

    set list on;
    set term ^;
    execute block returns(deleted_count int) as
    begin
      deleted_count = 0;
      for
        select id
        from tm
        order by id+0 -- "+0" is mandatory for getting BCA
        for update with lock -- and this also is mandatory
        as cursor c
      do begin
        delete from tm where current of c;
        deleted_count = deleted_count + 1;
      end
      suspend;
    end^
    set term ;^

    set count on;
    set echo on;
    select * from tm;
    rollback;
    select * from tm;
"""

act = isql_act('db', test_script)

expected_stdout = """
    DELETED_COUNT                   2
    select * from tm;
    Records affected: 0
    rollback;
    select * from tm;
    ID                              1
    ID                              2
    Records affected: 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

