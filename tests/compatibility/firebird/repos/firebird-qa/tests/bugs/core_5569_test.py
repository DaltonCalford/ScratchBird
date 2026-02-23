#coding:utf-8

"""
ID:          issue-5836
ISSUE:       5836
TITLE:       ISQL incorrectly pads UNICODE_FSS/UTF8 columns when they use a collation
DESCRIPTION:
JIRA:        CORE-5569
FBTEST:      bugs.core_5569
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list OFF; -- for this test we have to check table-wise view rather than list
    select
        _utf8 '1234567890' collate unicode as f_with_collate,
        _utf8 '1234567890' as f_without_collate,
        '|' as d
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    F_WITH_COLLATE F_WITHOUT_COLLATE D
    ============== ================= ======
    1234567890     1234567890        |
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

