#coding:utf-8

"""
ID:          issue-1599
ISSUE:       1599
TITLE:       Crash on infinite mutual SP calls (instead of "Too many concurrent executions of the same request.")
DESCRIPTION:
JIRA:        CORE-4653
FBTEST:      bugs.core_4653
NOTES:
    [30.06.2025] pzotov
    Part of call stack ('At procedure <SP_NAME> line X col Y') must be supressed because its length is limited to 1024 characters
    and number of lines (together with interrupting marker '...') depends on length of procedure name that is called recursively.
    Difference of transactions before and after call to recursive SP must be checked to be sure that there was no crash.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure p03(a_i int) returns (z int) as
    begin
      z = 0 ;
      suspend;
    end^
    commit^

    create or alter procedure p02(a_i int) returns (z int) as
    begin
      z = (select z from p03(:a_i)) + 1;
      suspend;
    end^
    commit^

    create or alter procedure p03(a_i int) returns (z int) as
    begin
      z = (select z from p02(:a_i)) + 1;
      suspend;
    end^
    commit^

    create or alter procedure p01(a_i int) returns (z int) as
    begin
      z = (select z from p02(:a_i)) + 1;
      suspend;
    end^
    commit^
    set term ;^
"""

db = db_factory(init=init_script)

test_script = """
  -- 07.03.2015: updated expected_stderr - it was changed and now is identical in 2.5 & 3.0
  -- Old stderr:
  -- Statement failed, SQLSTATE = HY001
  -- Stack overflow.  The resource requirements of the runtime stack have exceeded the memory available to it.
  set list on;
  set term ^;
  execute block as
  begin
      rdb$set_context('USER_TRANSACTION', 'INIT_TX', current_transaction);
  end ^
  set term ;^

  select * from p01(1);
  select current_transaction - cast( rdb$get_context('USER_TRANSACTION', 'INIT_TX') as int) as tx_diff from rdb$database;
"""

substitutions = [ ('^((?!(SQLSTATE|Too many concurrent executions|TX_DIFF)).)*$', ''), ('[ \t]+', ' ') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 54001
    Too many concurrent executions of the same request
    TX_DIFF 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

