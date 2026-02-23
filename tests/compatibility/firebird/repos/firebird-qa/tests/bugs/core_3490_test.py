#coding:utf-8

"""
ID:          issue-3849
ISSUE:       3849
TITLE:       Concurrency problem when using named cursors
DESCRIPTION:
JIRA:        CORE-3490
FBTEST:      bugs.core_3490
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table my_table (a integer, b integer,c integer);
    insert into my_table(a,b,c) values (1,1,1);
    commit;
    set transaction no wait;

    set term ^ ;
    execute block as
      declare my_cursor cursor for
        ( select b from my_table
          where a = 1
          for update of b with lock
        );
      declare b integer;

    begin
      open my_cursor;
      fetch my_cursor into :b;

      update my_table set c = 2
      where a = 1;

      UPDATE MY_TABLE SET A = 0 WHERE A = 1;

      update my_table set b = 2
      where current of my_cursor;
    end
    ^
    set term ;^
    set list on;
    select * from my_table;
"""

act = isql_act('db', test_script)

expected_stdout = """
    A                               0
    B                               2
    C                               2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

