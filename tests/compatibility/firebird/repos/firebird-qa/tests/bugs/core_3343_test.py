#coding:utf-8

"""
ID:          issue-3709
ISSUE:       3709
TITLE:       RETURNING clause is not supported in positioned (WHERE CURRENT OF) UPDATE and DELETE statements
DESCRIPTION:
JIRA:        CORE-3343
FBTEST:      bugs.core_3343
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test_a(id integer, cnt integer);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set term ^;
    execute block
    as
      declare variable cnt integer;
    begin
       for select cnt from test_a
          where id=1
          into :cnt as cursor c
          do begin
             update test_a
                set cnt=cnt+1
              where current of c
              returning cnt into :cnt;
          end
    end
    ^
    set term ;^
    commit;

    -- from core-3709 (refactored: check results when operating with blob):
    recreate table test(
       i int
      ,b blob sub_type 1
    );
    insert into test(i,b) values(1, 'qwe');
    commit;

    set blob all;
    set list on;

    set term ^;
    execute block returns(vb_old blob, vb_new blob) as
       declare c cursor for (  select b from test where i = 1 );
       declare v_s varchar(20);
    begin
       open c;
       while (1=1) do
       begin
         fetch c into :v_s;
         if (row_count = 0) then leave;
         update test set b = reverse(b)
         where current of c
         returning upper(old.b), upper(new.b) into vb_old, vb_new
         ;
         suspend;
       end
       close c;
    end
    ^
    set term ;^
    commit;

"""

act = isql_act('db', test_script, substitutions=[('VB_.*', '')])

expected_stdout = """
    QWE
    EWQ
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

