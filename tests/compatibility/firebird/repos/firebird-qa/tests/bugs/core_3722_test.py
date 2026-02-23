#coding:utf-8

"""
ID:          issue-1397
ISSUE:       1397
TITLE:       IS NOT DISTINCT FROM NULL doesn't use index
DESCRIPTION:
JIRA:        CORE-3722
FBTEST:      bugs.core_3722
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test (f01 int);
    insert into test select iif(rand() < 0.5, null, rand()*1000) from rdb$types;
    commit;
    create index test_f01 on test (f01);
    set planonly;
    select * from test where f01 is null;
    select * from test where f01 is not distinct from null;
    select * from test where f01 is not distinct from null PLAN (test INDEX (test_f01));
    select * from test where f01 is not distinct from nullif('', '');
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (TEST INDEX (TEST_F01))
    PLAN (TEST INDEX (TEST_F01))
    PLAN (TEST INDEX (TEST_F01))
    PLAN (TEST INDEX (TEST_F01))
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_F01"))
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_F01"))
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_F01"))
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_F01"))
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
