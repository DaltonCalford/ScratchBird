#coding:utf-8

"""
ID:          issue-5725
ISSUE:       5725
TITLE:       INSERT into updatable view without explicit field list failed
DESCRIPTION:
JIRA:        CORE-5454
FBTEST:      bugs.core_5454
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v_test as select 1 x from rdb$database;
    commit;

    recreate table test1(id int, x int);
    recreate table test2(id int, x int);

    recreate view v_test as
    select * from test1
    union all
    select * from test2;

    set term ^;
    create trigger v_test_dml for v_test before insert as
      declare i integer;
    begin
      i = mod(new.id, 2);
      if (i = 0) then
        insert into test1 values (new.id, new.x);
      else if (i = 1) then
        insert into test2 values (new.id, new.x);
    end
    ^
    set term ;^
    commit;

    set count on;
    insert into v_test values(123, 321);

"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

