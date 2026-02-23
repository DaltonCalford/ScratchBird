#coding:utf-8

"""
ID:          issue-5804
ISSUE:       5804
TITLE:       Connections compressed and encrypted in MON$ATTACHMENTS table
DESCRIPTION:
JIRA:        CORE-5536
FBTEST:      bugs.core_5536
NOTES:
    [01.07.2025] pzotov
    Refactored: we have to check only rows which contain either 'sqltype' or 'SQLSTATE'.
    Added appropriate substitutions.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    select mon$wire_compressed, mon$wire_encrypted
    from mon$attachments
    where mon$attachment_id = current_connection
    ;
"""

substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    01: sqltype: 32764 BOOLEAN Nullable scale: 0 subtype: 0 len: 1
    02: sqltype: 32764 BOOLEAN Nullable scale: 0 subtype: 0 len: 1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

