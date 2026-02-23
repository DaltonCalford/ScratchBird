#coding:utf-8

"""
ID:          issue-3099
ISSUE:       3099
TITLE:       Common table expression context could be used with parameters
DESCRIPTION:
JIRA:        CORE-2699
FBTEST:      bugs.core_2699
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db_1 = db_factory()

test_script = """
    with cte_alias as (
        select 1 n from rdb$database
    )
    select * from cte_alias(10);
"""

act = isql_act('db_1', test_script, substitutions=[('-At line.*', '')])

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -204
    -Procedure unknown
    -CTE_ALIAS
    -At line 4, column 15
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -204
    -Procedure unknown
    -"CTE_ALIAS"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
