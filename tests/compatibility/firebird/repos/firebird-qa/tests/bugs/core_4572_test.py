#coding:utf-8

"""
ID:          issue-4889
ISSUE:       4889
TITLE:       Incorrect error for PSQL function when the number of actual arguments does not match the number of formal arguments
DESCRIPTION:
JIRA:        CORE-4572
FBTEST:      bugs.core_4572
NOTES:
    [30.09.2023] pzotov
    Expected error message became differ in FB 6.x, added splitting.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter function fn_multiplier ( a_val int, a_times int ) returns bigint
    as
    begin
        return a_val * a_times;
    end
    ^

    create or alter procedure sp_multiplier (a_val int, a_times int )
    returns (result bigint)
    as
    begin
        result = a_val * a_times;
    end
    ^
    set term ;^
    commit;

    -- Confirmed on WI-T3.0.0.31374 Firebird 3.0 Beta 1:
    -- Statement failed, SQLSTATE = 39000
    -- invalid request BLR at offset 50
    -- -function FN_MULTIPLIER could not be matched
    select fn_multiplier(191) as t from rdb$database;

    execute procedure sp_multiplier(191);
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 07001
    Dynamic SQL Error
    -Input parameter mismatch for function FN_MULTIPLIER

    Statement failed, SQLSTATE = 07001
    Dynamic SQL Error
    -Input parameter mismatch for procedure SP_MULTIPLIER
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 07001
    Parameter mismatch for function "PUBLIC"."FN_MULTIPLIER"
    -Parameter A_TIMES has no default value and was not specified or was specified with DEFAULT

    Statement failed, SQLSTATE = 07001
    Parameter mismatch for procedure "PUBLIC"."SP_MULTIPLIER"
    -Parameter A_TIMES has no default value and was not specified or was specified with DEFAULT
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
