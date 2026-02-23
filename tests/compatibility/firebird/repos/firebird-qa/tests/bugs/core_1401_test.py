#coding:utf-8

"""
ID:          issue-1819
ISSUE:       1819
TITLE:       Global temporary table instance may pick up not all indices
DESCRIPTION:
JIRA:        CORE-1401
FBTEST:      bugs.core_1401
NOTES:
    [25.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Minimal snapshot number for 6.x: 6.0.0.863, see letter from Adriano, 24.06.2025 23:24, commit:
    https://github.com/FirebirdSQL/firebird/commit/79ff650e5af7a0d6141e166b0cb8208ef211f0a7

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create global temporary table gtt_test (f1 int, f2 int, f3 int);
    create index idx1 on gtt_test (f1);
    create index idx2 on gtt_test (f2);
    create index idx3 on gtt_test (f3);
    drop index idx2;
    set list on;
    set plan on;
    insert into gtt_test values (1, 1, 1);
    select * from gtt_test where f1 = 1;
    select * from gtt_test where f2 = 1;
    select * from gtt_test where f3 = 1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (GTT_TEST INDEX (IDX1))
    F1 1
    F2 1
    F3 1
    PLAN (GTT_TEST NATURAL)
    F1 1
    F2 1
    F3 1
    PLAN (GTT_TEST INDEX (IDX3))
    F1 1
    F2 1
    F3 1
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."GTT_TEST" INDEX ("PUBLIC"."IDX1"))
    F1 1
    F2 1
    F3 1
    PLAN ("PUBLIC"."GTT_TEST" NATURAL)
    F1 1
    F2 1
    F3 1
    PLAN ("PUBLIC"."GTT_TEST" INDEX ("PUBLIC"."IDX3"))
    F1 1
    F2 1
    F3 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

