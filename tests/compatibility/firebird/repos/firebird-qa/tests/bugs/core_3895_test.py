#coding:utf-8

"""
ID:          issue-4231
ISSUE:       4231
TITLE:       High memory usage when PSQL code SELECT's from stored procedure which modified some data
DESCRIPTION:
  Test does <run_cnt> calls of selectable SP which performs DML inside itself.
  After every call we store value of db_info(fdb.isc_info_current_memory) as new element in list.
  After all calls finish we scan list for difference between adjacent elements which exceeds
  <max_mem_leak> threshold.
  Value of this threshold depends on FB engine version.

  On current FB versions memory usage is incremented (after every call of SP, w/o commit) by:
  1) ~ 14500 bytes for 3.0
JIRA:        CORE-3895
FBTEST:      bugs.core_3895
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure sp_main returns (id integer) as begin suspend; end
    ^
    create or alter procedure sp_select_and_insert (p_id integer) returns (id integer) as begin suspend; end
    ^
    commit
    ^
    recreate table test (id integer)
    ^

    create or alter procedure sp_select_and_insert (p_id integer) returns (id integer) as
    begin
      insert into test(id) values(:p_id);
      id = p_id;
      suspend;
    end
    ^

    create or alter procedure sp_main returns (id integer)
    as
      declare i int = 0;
    begin
      while (i < 1000) do begin
        select id from sp_select_and_insert(:i) into :id;
        i = i + 1;
      end
      suspend;
    end
    ^
    commit
    ^
    set term ;^
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    run_cnt = 20
    with act.db.connect() as con:
        c = con.cursor()
        mem_usage = []
        for i in range(0, run_cnt):
            c.execute('select id from sp_main')
            c.fetchall()
            mem_usage.append(con.info.current_memory)
    max_mem_leak = 16384 # FB 3+
    print(mem_usage)
    for i in range(2, run_cnt):
        m0 = mem_usage[i-1]
        m1 = mem_usage[i]
        if m1 - m0 >= max_mem_leak:
            pytest.fail(f'Unexpected memory leak: {m1-m0} bytes, exceeds threshold = {max_mem_leak}')
