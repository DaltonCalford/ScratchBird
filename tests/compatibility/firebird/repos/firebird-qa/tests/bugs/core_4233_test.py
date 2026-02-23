#coding:utf-8

"""
ID:          issue-4557
ISSUE:       4557
TITLE:       In PSQL modules with declared cursors engine could assign value to the wrong variable
DESCRIPTION:
JIRA:        CORE-4233
FBTEST:      bugs.core_4233
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure hidden_vars
      returns (out_a int, out_b1 int, out_b2 int)
    as
    declare a int;
    declare c cursor for
      (select
          coalesce( (select count(*) from rdb$relations), -1)
          -- Aliasing of expression results in derived tables is mandatory in 3.0
          -- otherwise get error about metadata updating:
          -- -no column name specified for column number 1 in derived table
          -- <missing arg #2 - possibly status vector overflow>
          as tabs_cnt -- <<<<<<<<<<<<<<< nb <<<<<<<<<<<
          from rdb$database
      );
    declare b int = 0;
    begin
      out_b1 = b;

      open c;
      fetch c into :a;
      close c;

      out_a = a;
      out_b2 = b;
      b = b + 1;
      suspend;
    end ^
    set term ; ^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    select out_b1, out_b2 from hidden_vars;
    select out_b1, out_b2 from hidden_vars;
    select out_b1, out_b2 from hidden_vars;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """      OUT_B1       OUT_B2
============ ============
           0            0
      OUT_B1       OUT_B2
============ ============
           0            0
      OUT_B1       OUT_B2
============ ============
           0            0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
