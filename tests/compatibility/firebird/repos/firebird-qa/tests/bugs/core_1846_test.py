#coding:utf-8

"""
ID:          issue-2275
ISSUE:       2275
TITLE:       Allow index walk (ORDER plan) when there is a composite index {A, B} and the query looks like WHERE A = ? ORDER BY B
DESCRIPTION:
JIRA:        CORE-1846
FBTEST:      bugs.core_1846
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(
       n1 int
      ,n2 int
    );
    commit;

    insert into test select rand()*100, rand()*100 from rdb$types;
    commit;

    create index test_n1_n2_asc on test(n1, n2);
    commit;
    create descending index test_n2_n1_desc on test(n2, n1);
    commit;

    set planonly;
    select * from test where n1 = ? order by n2 asc;
    select * from test where n2 = ? order by n1 desc;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout_5x = """
    PLAN (TEST ORDER TEST_N1_N2_ASC)
    PLAN (TEST ORDER TEST_N2_N1_DESC)
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."TEST" ORDER "PUBLIC"."TEST_N1_N2_ASC")
    PLAN ("PUBLIC"."TEST" ORDER "PUBLIC"."TEST_N2_N1_DESC")
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
