#coding:utf-8

"""
ID:          issue-3479
ISSUE:       3479
TITLE:       Cannot alter the domain after migrating from older versions
DESCRIPTION:
JIRA:        CORE-3101
FBTEST:      bugs.core_3101
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3101-ods11.fbk')

test_script = """
    show domain state;
    alter domain state set default 0;
    commit;
    show domain state;
"""

substitutions = [ ('[ \t]+', ' ') ] 
act = isql_act('db', test_script, substitutions = substitutions)

expected_out_5x = """
    STATE SMALLINT Nullable
    STATE SMALLINT Nullable
    default 0
"""

expected_out_6x = """
    PUBLIC.STATE SMALLINT Nullable
    PUBLIC.STATE SMALLINT Nullable
    default 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_out_5x if act.is_version('<6') else expected_out_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
