#coding:utf-8

"""
ID:          issue-6524
ISSUE:       6524
TITLE:       Change type of MON$ATTACHMENTS.MON$IDLE_TIMER and MON$STATEMENTS.MON$STATEMENT_TIMER to TIMESTAMP WITH TIME ZONE
DESCRIPTION:
JIRA:        CORE-6282
FBTEST:      bugs.core_6282
NOTES:
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    set list on;
    select a.mon$idle_timer, s.mon$statement_timer from mon$attachments a join mon$statements s using(mon$attachment_id) rows 0;
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 32754 TIMESTAMP WITH TIME ZONE Nullable scale: 0 subtype: 0 len: 12
    02: sqltype: 32754 TIMESTAMP WITH TIME ZONE Nullable scale: 0 subtype: 0 len: 12
"""

@pytest.mark.version('>=4.0.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
