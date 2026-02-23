#coding:utf-8

"""
ID:          issue-1967
ISSUE:       1967
TITLE:       Unnecessary index scan happens when the same index is mapped to both WHERE and ORDER BY clauses
DESCRIPTION:
JIRA:        CORE-1550
FBTEST:      bugs.core_1550
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
    recreate table test(id int);
    commit;
    insert into test(id) select r.rdb$relation_id from rdb$relations r;
    commit;
    create index test_id on test(id);
    commit;

    set planonly;
    select *
    from test
    where id < 10
    order by id;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (TEST ORDER TEST_ID)
"""
expected_stdout_6x = """
    PLAN ("PUBLIC"."TEST" ORDER "PUBLIC"."TEST_ID")
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

