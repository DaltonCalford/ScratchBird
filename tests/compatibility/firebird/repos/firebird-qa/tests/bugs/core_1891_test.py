#coding:utf-8

"""
ID:          issue-2322
ISSUE:       2322
TITLE:       SHOW VIEW shows non-sense information for view fields with expressions
DESCRIPTION:
JIRA:        CORE-1891
FBTEST:      bugs.core_1891
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

VIEW_DDL = 'select n, n * 2 from test'
test_script = f"""
    create table test (n integer);
    create view view_test (x, y) as
    {VIEW_DDL};
    show view view_test;
"""

substitutions = [ ('[ \t]+', ' '), ('=.*', '') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = f"""
    X                               INTEGER Nullable
    Y                               BIGINT Expression
    View Source:
    {VIEW_DDL}
"""

expected_stdout_6x = f"""
    View: PUBLIC.VIEW_TEST
    X                               INTEGER Nullable
    Y                               BIGINT Expression
    View Source:
    {VIEW_DDL}
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
