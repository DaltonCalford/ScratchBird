#coding:utf-8

"""
ID:          issue-5374
ISSUE:       5374
TITLE:       Metadata extration (ISQL -X): "CREATE PROCEDURE/FUNCTION" statement contains reference to column of table(s) that not yet exists if this procedure had parameter of such type when it was created
DESCRIPTION:
    Test creates database with table 'TEST' and standalone and packaged procedures and functions which have parameters or variables
    with referencing to the table 'TEST' column. Also, there are DB-level and DDL-level triggers with similar references.
    Then we extract metadata and save it into file as 'initial' text.
    After this we drop all objects and make attempt to APPLY just extracted metadata script. It should perform without errors.
    Finally, we extract metadata again and do COMPARISON of their current content and those which are stored 'initial' file.
JIRA:        CORE-5089
FBTEST:      bugs.core_5089
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow to specify THE SAME terminator twice,
    i.e.
    set term @; select 1 from rdb$database @ set term @; - will not compile ("Unexpected end of command" raises).
"""

import pytest
from difflib import unified_diff
from firebird.qa import *

init_script = """
    create domain dm_test int not null check ( value >=-1 );

    create table test(mode varchar(30), result dm_test);
    commit;

    set term ^;
    create procedure sp_test(
       i1 dm_test
      ,i2 type of dm_test
      ,i3 type of column test.result
    ) returns (
      o1 dm_test
      ,o2 type of dm_test
      ,o3 type of column test.result
    )
    as
      declare v1 dm_test = 3;
      declare v2 type of dm_test = 7;
      declare v3 type of column test.result = 9;
    begin
      o1 = v1 * i1;
      o2 = v2 * i2;
      o3 = v3 * i3;

      suspend;

    end
    ^

    create function fn_test(
       i1 dm_test
      ,i2 type of dm_test
      ,i3 type of column test.result
    ) returns type of column test.result
    as
      declare v1 dm_test = 11;
      declare v2 type of dm_test = 13;
      declare v3 type of column test.result = 17;
    begin
      return v1 * i1 + v2 * i2 + v3 * i3;
    end
    ^

    create package pg_test as
    begin
      procedure pg_proc(
         i1 dm_test
        ,i2 type of dm_test
        ,i3 type of column test.result
      ) returns (
        o1 dm_test
        ,o2 type of dm_test
        ,o3 type of column test.result
      );

      function pg_func(
         i1 dm_test
        ,i2 type of dm_test
        ,i3 type of column test.result
      ) returns type of column test.result;
    end
    ^

    create package body pg_test as
    begin
      procedure pg_proc(
         i1 dm_test
        ,i2 type of dm_test
        ,i3 type of column test.result
      ) returns (
        o1 dm_test
        ,o2 type of dm_test
        ,o3 type of column test.result
      ) as
        declare v1 dm_test = 19;
        declare v2 type of dm_test = 23;
        declare v3 type of column test.result = 29;
      begin

        o1 = v1 * i1;
        o2 = v2 * i2;
        o3 = v3 * i3;

        suspend;

      end

      function pg_func(
         i1 dm_test
        ,i2 type of dm_test
        ,i3 type of column test.result
      ) returns type of column test.result as
        declare v1 dm_test = 13;
        declare v2 type of dm_test = 17;
        declare v3 type of column test.result = 19;
      begin
        return v1 * i1 + v2 * i2 + v3 * i3;
      end

    end
    ^
    create or alter trigger trg_connect on connect as
        declare v1 dm_test = 19;
        declare v2 type of dm_test = 23;
        declare v3 type of column test.result = 29;
    begin
        /* 1st db-level trigger, on CONNECT event */
    end
    ^

    create or alter trigger trg_commit on transaction commit as
        declare v1 dm_test = 19;
        declare v2 type of dm_test = 23;
        declare v3 type of column test.result = 29;
    begin
        /* 2nd db-level trigger, on transaction COMMIT event */
    end
    ^

    create or alter trigger trg_ddl_before before any ddl statement
    as
        declare v1 dm_test = 19;
        declare v2 type of dm_test = 23;
        declare v3 type of column test.result = 29;
    begin
        /* DDL-level trigger before any ddl statement */
    end
    ^

    set term ;^
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

act = python_act('db')

ddl_clear_all = """
    drop trigger trg_ddl_before;
    drop trigger trg_commit;
    drop trigger trg_connect;
    drop package pg_test;
    drop function fn_test;
    drop procedure sp_test;
    drop table test;
    drop domain dm_test;
    commit;
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    # Extract metadata
    act.isql(switches=['-x'], combine_output = True)
    initial_metadata = act.stdout
    # Clear all
    act.reset()

    act.isql(switches=[], input=ddl_clear_all, combine_output = True)
    # Apply extracted metadata
    act.reset()

    act.isql(switches=[], input=initial_metadata, combine_output = True)
    # Extract new metadata
    act.reset()

    act.isql(switches=['-x'], combine_output = True)
    new_metadata = act.stdout

    # Check
    assert list(unified_diff(initial_metadata, new_metadata)) == []
