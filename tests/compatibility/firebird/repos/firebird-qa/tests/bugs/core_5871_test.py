#coding:utf-8

"""
ID:          issue-6130
ISSUE:       6130
TITLE:       Incorrect caching of the subquery result (procedure call) in independent queries
DESCRIPTION:
  Beside stanalone stored procedure it was decided to check also stored function, and packaged SP and func.
JIRA:        CORE-5871
FBTEST:      bugs.core_5871
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure p1 (n int) returns (r int) as
    begin
      r = n;
      suspend;
    end
    ^

    create or alter function f1 (n int) returns int as
      declare r int;
    begin
      r = n;
      return r;
    end
    ^

    create or alter package pkg
    as
    begin
        procedure pk_p1(n int) returns(r int);
        function pk_f1(n int) returns int;
    end
    ^

    recreate package body pkg
    as
    begin
        procedure pk_p1(n int) returns(r int) as
        begin
          r = n;
          suspend;
        end

        function pk_f1(n int) returns int as
          declare r int;
        begin
          r = n;
          return r;
        end
    end
    ^

    --------------------------------------------------------------------------------


    create or alter procedure sp_test returns ( s1 varchar(100), s2 varchar(100), s3 varchar(100), s4 varchar(100) ) as
      declare i int;
    begin
      i = 0;
      while (i < 3) do
      begin
        i = i + 1;

        select
          (select
             coalesce(:s1, '') || ' ' || :i || '=' || (select r from p1(:i))
             from rdb$database
          )
          from rdb$database
        into s1;

        select
          (
             select
             coalesce(:s2, '') || ' ' || :i || '=' || ( select f1(:i) from rdb$database )
             from rdb$database
          )
          from rdb$database
        into s2;


        select
          (select
             coalesce(:s3, '') || ' ' || :i || '=' || (select r from pkg.pk_p1(:i))
             from rdb$database
          )
          from rdb$database
        into s3;

        select
          (
             select
             coalesce(:s4, '') || ' ' || :i || '=' || ( select pkg.pk_f1(:i) from rdb$database )
             from rdb$database
          )
          from rdb$database
        into s4;


      end
      suspend;
    end
    ^
    set term ;^
    commit;

    set list on;
    select * from sp_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    S1                               1=1 2=2 3=3
    S2                               1=1 2=2 3=3
    S3                               1=1 2=2 3=3
    S4                               1=1 2=2 3=3
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
