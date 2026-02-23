#coding:utf-8

"""
ID:          issue-5668
ISSUE:       5668
TITLE:       Invalid data type for negation (minus operator)
DESCRIPTION:
JIRA:        CORE-5395
FBTEST:      bugs.core_5395
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set  list on;
    set term ^;
    execute block returns (eval_result int) as
    begin
        for
            execute statement ('select 1 from rdb$database where 1 = - :id') (id := -1)
        into :eval_result
        do
            suspend;

        -- Statement failed, SQLSTATE = 42000
        -- Dynamic SQL Error
        -- -expression evaluation not supported
        -- -Invalid data type for negation (minus operator)
        -- -At block line: 3, col: 5

    end
    ^

    execute block returns (eval_result int) as
    begin
        for
            execute statement ('select 1 from rdb$database where 1 = (:id) * -1') (id := -1)
        into :eval_result
        do
            suspend;

        -- Statement failed, SQLSTATE = 42000
        -- Dynamic SQL Error
        -- -expression evaluation not supported
        -- -Invalid data type for multiplication in dialect N, N=1 or 3
        -- -At block line: 13, col: 3

    end
    ^

    execute block returns (eval_result int) as
       declare selected_year int;
       declare selected_mont int;
    begin
        selected_year = extract(year from current_timestamp);
        selected_mont = extract(month from current_timestamp);

        for
            execute statement ('select 1 from rdb$database where extract(year from current_timestamp)*100 + extract(month from current_timestamp) = ? * 100 + ? ') (selected_year, selected_mont)
        into :eval_result
        do
            suspend;

        -- Statement failed, SQLSTATE = 42000
        -- Dynamic SQL Error
        -- -expression evaluation not supported
        -- -Invalid data type for multiplication in dialect N, N=1 or 3
        -- -At block line: 8, col: 5

    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    EVAL_RESULT                     1
    EVAL_RESULT                     1
    EVAL_RESULT                     1
"""

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

