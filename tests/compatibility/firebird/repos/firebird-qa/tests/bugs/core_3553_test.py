#coding:utf-8

"""
ID:          issue-3909
ISSUE:       3909
TITLE:       Nested loop plan is chosen instead of the sort merge for joining independent
  streams using keys of different types
DESCRIPTION:
JIRA:        CORE-3553
FBTEST:      bugs.core_3553
NOTES:
    [07.04.2022] pzotov
    FB 5.0.0.455 and later: data sources with equal cardinality now present in the HASH plan in order they are specified in the query.
    Reversed order was used before this build. Because of this, two cases of expected stdout must be taken in account, see variables
    'fb3x_checked_stdout' and 'fb5x_checked_stdout'.

    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    select count(*)
    from rdb$database d1
    join rdb$database d2  on cast(d1.rdb$relation_id as char(10)) = cast(d2.rdb$relation_id as char(20));
"""

act = isql_act('db', test_script)

expected_stdout_4x = """
    PLAN HASH (D2 NATURAL, D1 NATURAL)
"""

expected_stdout_5x = """
    PLAN HASH (D1 NATURAL, D2 NATURAL)
"""

expected_stdout_6x = """
    PLAN HASH ("D1" NATURAL, "D2" NATURAL)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

