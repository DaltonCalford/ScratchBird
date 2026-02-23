#coding:utf-8

"""
ID:          issue-3811
ISSUE:       3811
TITLE:       Inefficient optimization (regression)
DESCRIPTION:
JIRA:        CORE-3450
FBTEST:      bugs.core_3450
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_2 as begin end;

    recreate table test_1 (f1 int, f2 int, f3 int);
    create index test_1_f1 on test_1(f1);
    create index test_1_f2 on test_1(f2);
    commit;

    recreate table test_2 (f1 int, f2 int);
    create index test_2_f1 on test_2(f1);
    create index test_2_f2 on test_2(f2);
    commit;

    recreate table test_3 (f1 int);
    create index test_3_f1 on test_3(f1);
    commit;

    set term ^;
    create or alter procedure sp_1 returns (f1 int)
    as begin
      f1=1;
      suspend;
    end
    ^

    create or alter procedure sp_2 as
      declare i int;
      declare t1_lim int = 1000;
      declare t2_lim int = 100;
      declare t3_lim int = 10;
    begin
      i=0;
      while (i<t1_lim) do begin
        i=i+1;
        insert into test_1 values (:i, 1, 3);
      end

      i=0;
      while (i<t2_lim) do begin
        i=i+1;
        insert into test_2 values (:i, 2);
      end

      i=0;
      while (i<t3_lim) do begin
        i=i+1;
        insert into test_3 values (3);
      end
    end
    ^
    set term ;^
    commit;

    execute procedure sp_2;
    commit;

    set statistics index test_1_f1;
    set statistics index test_1_f2;
    set statistics index test_2_f1;
    set statistics index test_2_f2;
    set statistics index test_3_f1;
    commit;

    set planonly;

    select t2.f1
    from test_2 t2
    join test_1 t1 on t1.f1=t2.f1
    join sp_1 p1 on p1.f1=t1.f2
    join test_3 t3 on t3.f1=t1.f3
    where t2.f2=2
    ;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN JOIN (JOIN (P1 NATURAL, T1 INDEX (TEST_1_F2)), T2 INDEX (TEST_2_F1), T3 INDEX (TEST_3_F1))
"""

expected_stdout_6x = """
    PLAN JOIN (JOIN ("P1" NATURAL, "T1" INDEX ("PUBLIC"."TEST_1_F2")), "T2" INDEX ("PUBLIC"."TEST_2_F1"), "T3" INDEX ("PUBLIC"."TEST_3_F1"))
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
