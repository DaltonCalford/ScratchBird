#coding:utf-8

"""
ID:          issue-2869
ISSUE:       2869
TITLE:       Server fails when doing DROP DATABASE right after error in statistical fnction
DESCRIPTION:
JIRA:        CORE-2455
FBTEST:      bugs.core_2455
"""

import pytest
from firebird.qa import *

db = db_factory(charset='ISO8859_1')

test_script = """
    commit;
    create database 'localhost:$(DATABASE_LOCATION)trucks';

    set term ^;
    create procedure factorial (
        max_rows integer,
        mode integer )
    returns (
        row_num integer,
        result integer
    ) as
        declare variable temp integer;
        declare variable counter integer;
    begin
      counter=0;
      temp=1;
      while (counter <= max_rows) do
      begin
        row_num = counter;

        if ( row_num = 0 ) then
            temp = 1;
        else
            temp = temp * row_num;

        result = temp;
        counter = counter + 1;

        if (mode=1) then
            suspend;
      end

      if (mode = 2) then
          suspend;
    end
    ^
    set term ; ^
    commit;

    set list on;

    select 0 as run_no, sp.* from factorial(5,2) sp;

    create table onerow (i integer);
    insert into onerow values (5);

    -- note the derived table query
    select
        1 as run_no
       ,(select ROW_NUM from factorial(i,2)) as RN
    from onerow ;

    select
        2 as run_no
       ,(select RESULT from factorial(i,2)) as RS
    from onerow ;

    -- note the derived table query
    select 3 as run_no, (select ROW_NUM from factorial(i,1)) as RN from onerow ;
    select 4 as run_no, (select RESULT from factorial(i,1)) as RS from onerow ;

    drop database ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RUN_NO                          0
    ROW_NUM                         5
    RESULT                          120
    RUN_NO                          1
    RN                              5
    RUN_NO                          2
    RS                              120
"""

expected_stderr = """
    Statement failed, SQLSTATE = 21000
    multiple rows in singleton select
    Statement failed, SQLSTATE = 21000
    multiple rows in singleton select
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

