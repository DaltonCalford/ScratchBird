#coding:utf-8

"""
ID:          issue-2994
ISSUE:       2994
TITLE:       Wrong results for CASE used together with GROUP BY
DESCRIPTION:
JIRA:        CORE-2584
FBTEST:      bugs.core_2584
"""

import pytest
from firebird.qa import *

db_1 = db_factory(charset='UTF8')

test_script = """
    -- 08.03.2015: removed code based on data in RDB$ tables - they differs in 2.5 vs 3.0.
    recreate table rf(rp int);
    commit;

    set term ^;
    execute block as
      declare n int = 250;
    begin
      delete from rf;
      while (n>0) do insert into rf(rp) values( mod(:n, 1+mod(:n,7) ) )returning :n-1 into n;
    end
    ^ set term ;^
    commit;

    set list on;
    select
      rp as a,
      case rp
        when 0 then '0'
        when 1 then '1'
        when 2 then '2'
        when 3 then '3'
        when 4 then '4'
      end as b,
      count(*) as cnt
    from rf
    where rp < 5
    group by 1, 2;
"""

act = isql_act('db_1', test_script)

expected_stdout = """
    A                               0
    B                               0
    CNT                             87
    A                               1
    B                               1
    CNT                             52
    A                               2
    B                               2
    CNT                             34
    A                               3
    B                               3
    CNT                             22
    A                               4
    B                               4
    CNT                             14
"""

@pytest.mark.version('>=2.5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

