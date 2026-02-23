#coding:utf-8

"""
ID:          issue-5803
ISSUE:       5803
TITLE:       Garbage value in RDB$FIELD_SUB_TYPE in RDB$FUNCTION_ARGUMENTS after altering function
DESCRIPTION:
JIRA:        CORE-5535
FBTEST:      bugs.core_5535
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter view v_check as
    select rdb$field_sub_type from rdb$function_arguments where rdb$function_name=upper('test')
    ;
    commit;

    set list on;
    set count on;

    select * from v_check;

    create function test(i int) returns int as begin end;
    commit;
    select * from v_check;

    create or alter function test(i int) returns int as begin end;
    commit;
    select * from v_check;

"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
    RDB$FIELD_SUB_TYPE              <null>
    RDB$FIELD_SUB_TYPE              <null>
    Records affected: 2
    RDB$FIELD_SUB_TYPE              <null>
    RDB$FIELD_SUB_TYPE              <null>
    Records affected: 2
"""

@pytest.mark.version('>=3.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

