#coding:utf-8

"""
ID:          issue-2045
ISSUE:       2045
TITLE:       MERGE not correctly worked with parameters in MATCHING clause
DESCRIPTION:
JIRA:        CORE-1624
FBTEST:      bugs.core_1624
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table src(id int, x int);
    recreate table tgt(id int, x int);
    commit;
    insert into src values(1, 100);
    insert into src values(2, 200);
    insert into src values(3, 300);
    insert into src values(4, 400);
    commit;
    insert into tgt values(2, 10);
    insert into tgt values(3, 20);
    commit;
    set term ^;
    execute block as
      declare v_stt varchar(255);
    begin
      v_stt =
          'merge into tgt t using src s on s.id = t.id '
          || 'when matched then update set t.x = s.x + ?'
          || 'when NOT matched then insert values(s.id, s.id + ?)';

      execute statement (v_stt) ( 1000, 20000 );
    end
    ^
    set term ;^
    select * from tgt;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
              ID            X
    ============ ============
               2         1200
               3         1300
               1        20001
               4        20004
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

