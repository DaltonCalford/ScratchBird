#coding:utf-8

"""
ID:          issue-2478
ISSUE:       2478
TITLE:       Connection lost to database when used AUTONOMOUS TRANSACTION
DESCRIPTION:
JIRA:        CORE-2042
FBTEST:      bugs.core_2042
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure test_caller_name
    returns (
        object_name char(31),
        object_type smallint)
    as
    begin
    end
    ^
    create or alter procedure get_caller_name
    returns (
        object_name char(31),
        object_type smallint)
    as
    declare variable tran_id integer;
    begin
      tran_id = current_transaction;

      in autonomous transaction do
      begin
        select cs.mon$object_name, cs.mon$object_type
          from mon$call_stack cs, mon$statements st
         where cs.mon$statement_id = st.mon$statement_id
               and st.mon$transaction_id = :tran_id
               and cs.mon$caller_id is null -- :: nb :: Added this condition because output differs in 3.0 vs 2.5!
          order by cs.mon$call_id
          rows 1
        into :object_name, :object_type;
      end

      suspend;
    end
    ^

    create or alter procedure test_caller_name
    returns (
        object_name char(31),
        object_type smallint)
    as
    begin
      select object_name, object_type from get_caller_name
         into :object_name, :object_type;

      suspend;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select * from test_caller_name;
    select * from test_caller_name;
    select * from test_caller_name;
    select * from test_caller_name;
    select * from test_caller_name;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    OBJECT_NAME                     TEST_CALLER_NAME
    OBJECT_TYPE                     5
    OBJECT_NAME                     TEST_CALLER_NAME
    OBJECT_TYPE                     5
    OBJECT_NAME                     TEST_CALLER_NAME
    OBJECT_TYPE                     5
    OBJECT_NAME                     TEST_CALLER_NAME
    OBJECT_TYPE                     5
    OBJECT_NAME                     TEST_CALLER_NAME
    OBJECT_TYPE                     5
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

