#coding:utf-8

"""
ID:          issue-4767
ISSUE:       4767
TITLE:       Positioned UPDATE statement prohibits index usage for the subsequent cursor field references
DESCRIPTION:
JIRA:        CORE-4447
FBTEST:      bugs.core_4447
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
  recreate table ts(id int, x int, y int, z int, constraint ts_pk_id primary key (id) );
  recreate table tt(x int, y int, z int, constraint tt_pk_xy primary key (x,y) );
  commit;

  insert into ts
  select row_number()over(), rand()*10, rand()*10, rand()*10
  from rdb$types;
  commit;

  insert into tt select distinct x,y,0 from ts;
  commit;
"""

db = db_factory(init=init_script)

test_script = """
  set planonly;
  set term ^;
  execute block as
  begin
    for select id,x,y,z from ts as cursor c
    do begin
       update ts set id = id where current of c; -- <<<<<<<<<<<<<<< ::: NB ::: we lock record in source using "current of" clause
       update tt t set t.z = t.z + c.z where t.x=c.x and t.y = c.y;
    end
  end
  ^ set term ;^
  set planonly;
"""

act = isql_act('db', test_script, substitutions = [('(--\\s+)?line \\d+, col(umn)? \\d+', '')])

expected_stdout_5x = """
    PLAN (T INDEX (TT_PK_XY))
    PLAN (C TS NATURAL)
"""

expected_stdout_6x = """
    PLAN ("T" INDEX ("PUBLIC"."TT_PK_XY"))
    PLAN ("C" "PUBLIC"."TS" NATURAL)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
