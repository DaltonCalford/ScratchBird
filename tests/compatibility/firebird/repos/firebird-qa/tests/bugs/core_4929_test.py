#coding:utf-8

"""
ID:          issue-5220
ISSUE:       5220
TITLE:       Cannot compile source with "ELSE IF ( <expr> ) THEN" statement and commands
  to manupulate explicit cursor inside
DESCRIPTION:
JIRA:        CORE-4929
FBTEST:      bugs.core_4929
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure sp_test(v smallint) returns(result int) as
    begin
        result = null;
        suspend;
    end
    ^
    set term ;^

    commit;

    set term ^;
    alter procedure sp_test(v smallint) returns(result int) as

      declare c1 cursor for (
         select 1 id from rdb$database
      );

      declare c2 cursor for (
         select 2 id from rdb$database
      );

    begin

        if ( v = 1 ) then open c1;
        else
            if ( :v = 2 ) then
                open c2;

        while (1=1) do
        begin
            if ( v = 1 ) then fetch c1 into result;
            else
                if ( :v = 2 ) then
                    fetch c2 into result;

            if (row_count = 0) then leave;

            suspend;

        end

        if ( v = 1 ) then close c1;
        else
            if ( :v = 2 ) then
                close c2;

    end
    ^
    set term ;^
    commit;

    set list on;
    select * from sp_test(1);
    select * from sp_test(2);

"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT                          1
    RESULT                          2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

