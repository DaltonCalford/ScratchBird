#coding:utf-8

"""
ID:          issue-4737
ISSUE:       4737
TITLE:       Useless extraction of generic DDL trigger
DESCRIPTION:
JIRA:        CORE-4415
FBTEST:      bugs.core_4415
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter trigger tr before any ddl statement as begin end;
    show trigger tr;
    -- Confirmed excessive output in WI-T3.0.0.30809 Firebird 3.0 Alpha 2. Was:
    -- TR, Sequence: 0, Type: BEFORE CREATE TABLE OR ALTER TABLE OR DROP TABLE OR ... OR <unknown>, Active // length = 967 characters.
"""

substitutions = [('\\+.*', ''), ('\\=.*', ''), ('Trigger text.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    TR, Sequence: 0, Type: BEFORE ANY DDL STATEMENT, Active
    as begin end
"""

expected_stdout_6x = """
    PUBLIC.TR, Sequence: 0, Type: BEFORE ANY DDL STATEMENT, Active
    as begin end
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
