#coding:utf-8

"""
ID:          issue-6304
ISSUE:       6304
TITLE:       Random crash 64bit fb_inet_server. Possible collation issue
DESCRIPTION:
    Only *one* error message should raise.
    Output should finish on: 'Records affected: 0', see:
    https://github.com/FirebirdSQL/firebird/issues/6304#issuecomment-826244780
JIRA:        CORE-6054
FBTEST:      bugs.core_6054
NOTES:
    [02.07.2025] pzotov
    Refactored: added subst to suppress name of non-existing column as it has no matter for this test.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table c (id int, f1 varchar(32) character set win1251 collate win1251);
    select * from c where non_existing_column collate win1251_ua = 'x';
    set count on;
    select * from c where f1 = _utf8 'x';
"""

substitutions = [('(-)?At line.*', ''), ('(-)?(")?NON_EXISTING_COLUMN(")?', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown

    Records affected: 0
"""

@pytest.mark.version('>=2.5.9')
def test_1(act: Action):

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
