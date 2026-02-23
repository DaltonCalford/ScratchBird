#coding:utf-8

"""
ID:          issue-7537
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7537
TITLE:       Wrong name in error message when unknown namespace is passed into RDB$SET_CONTEXT()
NOTES:
    [03.04.2023] pzotov
    Checked on intermediate snapshots 4.0.3.2920, 5.0.0.1001.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    -- 1. Trying to set non-existent, empty or non-textual namespace:
    select rdb$set_context('NO_SUCH_NS', 'VARNAME', 2) from rdb$database;
    select rdb$set_context('', 'VARNAME', 2) from rdb$database;
    select rdb$set_context(null, 'VARNAME', 2) from rdb$database;
    select rdb$set_context(false, 'VARNAME', 2) from rdb$database;
    select rdb$set_context(date '05.04.2023', 'VARNAME', 2) from rdb$database;
    select rdb$set_context(1, 'VARNAME', 2) from rdb$database;

    -- 2. Trying to get smth non-existent, empty or non-textual namespace:
    select rdb$get_context('NO_SUCH_NS', 'VARNAME') from rdb$database;
    select rdb$get_context('', 'VARNAME') from rdb$database;
    select rdb$get_context(null, 'VARNAME') from rdb$database;
    select rdb$get_context(false, 'VARNAME') from rdb$database;
    select rdb$get_context(date '05.04.2023', 'VARNAME') from rdb$database;
    select rdb$get_context(1, 'VARNAME') from rdb$database;
    
    -- 3. Trying to set smth in the READ-ONLY name space 'SYSTEM':
    select rdb$set_context('SYSTEM', 'NO_SUCH_VAR', 2) from rdb$database;
    select rdb$set_context('SYSTEM', '', 2) from rdb$database;

    -- 4. Trying to get non-existent or empty variable from 'SYSTEM':
    select rdb$get_context('SYSTEM', 'NO_SUCH_VAR') from rdb$database;
    select rdb$get_context('SYSTEM', '') from rdb$database;
"""

expected_stdout = """
    Statement failed, SQLSTATE = HY000
    Invalid namespace name 'NO_SUCH_NS' passed to RDB$SET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name '' passed to RDB$SET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid argument passed to RDB$SET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name 'FALSE' passed to RDB$SET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name '2023-04-05' passed to RDB$SET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name '1' passed to RDB$SET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name 'NO_SUCH_NS' passed to RDB$GET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name '' passed to RDB$GET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid argument passed to RDB$GET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name 'FALSE' passed to RDB$GET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name '2023-04-05' passed to RDB$GET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name '1' passed to RDB$GET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name 'SYSTEM' passed to RDB$SET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Invalid namespace name 'SYSTEM' passed to RDB$SET_CONTEXT

    Statement failed, SQLSTATE = HY000
    Context variable 'NO_SUCH_VAR' is not found in namespace 'SYSTEM'

    Statement failed, SQLSTATE = HY000
    Context variable '' is not found in namespace 'SYSTEM'
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.11')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
