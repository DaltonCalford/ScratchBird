#coding:utf-8

"""
ID:          issue-4422
ISSUE:       4422
TITLE:       Wrong parameters order in trace output
DESCRIPTION:
NOTES:
[07.07.2016]
  WI-T4.0.0.238 will issue in trace log following parametrized statement:
  ===
  with recursive role_tree as (
    select rdb$relation_name as nm, 0 as ur from rdb$user_privileges
    where
      rdb$privilege = 'M' and rdb$field_name = 'D'
      and rdb$user = ?  and rdb$user_type = 8
    union all
    select rdb$role_name as nm, 1 as ur from rdb$roles
    where rdb$role_name =...

  param0 = varchar(93), "SYSDBA"
  param1 = varchar(93), "NONE"
  ===

  This statement will issued BEFORE our call of stored procedure, so we have to analyze
  lines from trace only AFTER we found pattern 'execute procedure sp_test'.
JIRA:        CORE-4094
FBTEST:      bugs.core_4094
"""

import pytest
import re
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure sp_test(a int, b int, c int, d int) as
        declare n int;
    begin
          execute statement (
              'select
                  (select 123 from rdb$database where rdb$relation_id=:a)
              from rdb$database
              cross join
              (
                  select 123 as pdk
                  from rdb$database
                  where rdb$relation_id=:b and rdb$relation_id=:c and rdb$relation_id=:d
              )
              rows 1'
          )
          ( a := :a, b := :b, c := :c, d := :d )
          into n;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
    param0 = smallint, "1"
    param1 = smallint, "2"
    param2 = smallint, "3"
    param3 = smallint, "4"
"""

trace = ['time_threshold = 0',
         'log_statement_start = true',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    with act.trace(db_events=trace):
        act.isql(switches=['-n', '-q'], input='execute procedure sp_test(1, 2, 3, 4);')
    # process trace
    spcall_pattern = re.compile("execute procedure ")
    params_pattern = re.compile("param[0-9]{1} = ")
    flag = False
    for line in act.trace_log:
        if spcall_pattern.match(line):
            flag = True
        if flag and params_pattern.match(line):
            print(line)
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
