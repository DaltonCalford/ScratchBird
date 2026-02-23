#coding:utf-8

"""
ID:          issue-5231
ISSUE:       5231
TITLE:       Add label about deterministic flag for stored function in SHOW and extract commands
DESCRIPTION:
JIRA:        CORE-4940
FBTEST:      bugs.core_4940
NOTES:
    [12.12.2023] pzotov
    Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

FUNC_DDL_1 = """
    begin
        return 9223372036854775807;
    end
"""

FUNC_DDL_2 = """
    begin
        return rand() * 9223372036854775807;
    end
"""

test_script = f"""
    -- 1. Specify 'deterministic' flag - it should be reflected in SHOW command:
    set term ^;
    create or alter function fn_test returns bigint deterministic as
    {FUNC_DDL_1}
    ^
    set term ;^
    commit;

    show function fn_test;

    -- 2. Remove 'deterministic' flag - it also should be reflected in SHOW command:
    set term ^;
    alter function fn_test returns bigint as
    {FUNC_DDL_2}
    ^
    set term ;^
    commit;

    show function fn_test;
"""

substitutions = [('===.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = f"""
        Deterministic function
        Function text:
        {FUNC_DDL_1}

        Parameters:
        OUTPUT BIGINT

        Function text:
        {FUNC_DDL_2}

        Parameters:
        OUTPUT BIGINT
    """

    expected_stdout_6x = f"""
        Function: PUBLIC.FN_TEST 
        Deterministic function
        Function text:
        {FUNC_DDL_1}

        Parameters:
        OUTPUT BIGINT

        Function: PUBLIC.FN_TEST 
        Function text:
        {FUNC_DDL_2}

        Parameters:
        OUTPUT BIGINT
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
