#coding:utf-8

"""
ID:          issue-3130
ISSUE:       3130
TITLE:       isql hangs when trying to show a view based on a procedure
DESCRIPTION:
JIRA:        CORE-2735
FBTEST:      bugs.core_2735
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create procedure sp_test returns(o_result int) as begin o_result = 9; suspend; end^
    create view vp1 as select o_result from sp_test^
    set term ;^
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """
    show view vp1;
"""

substitutions = [('==.*', ''), ('[ \t]+', ' ')]

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    O_RESULT INTEGER Nullable
    View Source:
    select o_result from sp_test
"""

expected_stdout_6x = """
    View: PUBLIC.VP1
    O_RESULT INTEGER Nullable
    View Source:
    select o_result from sp_test
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
