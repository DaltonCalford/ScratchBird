#coding:utf-8

"""
ID:          issue-2141
ISSUE:       2141
TITLE:       Wrong variable initialization in recursive procedures
DESCRIPTION:
JIRA:        CORE-1716
FBTEST:      bugs.core_1716
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_int as integer default 0 not null;
    commit;

    -- sample with returned variable
    set term ^;
    create procedure sp_test1(a_cnt int)
    returns (o_cnt int, o_ret dm_int)
    as
    begin
      while (a_cnt>0) do
      begin
        o_cnt = a_cnt;
        a_cnt = a_cnt-1;
        for
          select o_ret
          from sp_test1( :a_cnt )
          into o_ret
        do
          suspend;
      end
      suspend;
    end
    ^

    create procedure sp_test2(a_cnt int)
    returns (o_cnt int, o_ret dm_int)
    as
    declare x dm_int;
    begin
      while (a_cnt>0) do
      begin
        o_cnt = a_cnt;
        a_cnt = a_cnt-1;
        for
          select o_ret
          from sp_test2( :a_cnt )
          into o_ret
          do begin
            o_ret = x;
            suspend;
          end
      end
      o_ret=x;
      suspend;
    end
    ^
    set term ;^
    commit;

    select * from sp_test1(3);
    select * from sp_test2(3);
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
           O_CNT        O_RET
    ============ ============
               3            0
               3            0
               3            0
               3            0
               2            0
               2            0
               1            0
               1            0


           O_CNT        O_RET
    ============ ============
               3            0
               3            0
               3            0
               3            0
               2            0
               2            0
               1            0
               1            0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

