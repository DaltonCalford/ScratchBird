#coding:utf-8

"""
ID:          issue-7676
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7676
TITLE:       Invalid message when violating unique constraint ("Attempt to evaluate index expression recursively")
DESCRIPTION:
NOTES:
    [28.05.2024] pzotov
        Confirmed bug on 5.0.0.1111: got SQLSTATE = HY000 / Attempt to evaluate index expression recursively
        Checked on 5.0.0.1121.
    [04.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.909; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test(x int);
    create unique index test_unq on test computed by (x);
    commit;
    insert into test values(1);
    insert into test values(1);
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=5.0.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {SQL_SCHEMA_PREFIX}"TEST_UNQ"
        -Problematic key value is (<expression> = 1)
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
