#coding:utf-8

"""
ID:          issue-1978
ISSUE:       1978
TITLE:       Dropping NOT NULL contranint doesn'have the desired effect
DESCRIPTION:
JIRA:        CORE-1559
FBTEST:      bugs.core_1559
NOTES:
    [25.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (n integer constraint explicit_check_for_nn not null);
    insert into test values (null);
    commit;
    alter table test drop constraint explicit_check_for_nn;
    insert into test values (null);
    select * from test;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."N", value "*** null ***"
    N <null>
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST"."N", value "*** null ***"
    N <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
