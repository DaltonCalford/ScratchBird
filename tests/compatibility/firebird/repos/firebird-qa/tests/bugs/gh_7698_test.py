#coding:utf-8

"""
ID:          issue-7698
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7698
TITLE:       The legacy plan with window functions is broken
DESCRIPTION:
NOTES:
        Confirmed bug on 5.0.0.1149
        Checked on 5.0.0.1155 -- all OK.
    [05.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = f"""
    set plan on;
    set planonly;
    select count(*) over() from rdb$relations;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = """
        PLAN (RDB$RELATIONS NATURAL)
    """
    expected_stdout_6x = """
        PLAN ("SYSTEM"."RDB$RELATIONS" NATURAL)
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
