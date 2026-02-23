#coding:utf-8

"""
ID:          issue-2242
ISSUE:       2242
TITLE:       Index is not used for some date/time expressions in dialect 1
DESCRIPTION:
JIRA:        CORE-1812
FBTEST:      bugs.core_1812
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    create table test (dts timestamp) ;
    commit;
    insert into test
    select dateadd( rand() * 10 second to localtimestamp )
    from rdb$types, rdb$types;
    commit;
    create index test_dts on test(dts);
    commit;

    set planonly;
    select * from test where dts = localtimestamp;
    select * from test where dts = current_timestamp;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (TEST INDEX (TEST_DTS))
    PLAN (TEST INDEX (TEST_DTS))
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_DTS"))
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_DTS"))
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
