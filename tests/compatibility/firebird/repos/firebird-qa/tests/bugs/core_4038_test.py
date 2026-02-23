#coding:utf-8

"""
ID:          issue-4368
ISSUE:       4368
TITLE:       Broken optimization for the stored dbkeys
DESCRIPTION:
JIRA:        CORE-4038
FBTEST:      bugs.core_4038
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table t (dbkey char(8) character set octets);
    create index it on t (dbkey);

    SET PLANONLY;
    select * from t as t1
    left join t as t2 on t2.dbkey = t1.rdb$db_key;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN JOIN (T1 NATURAL, T2 INDEX (IT))
"""

expected_stdout_6x = """
    PLAN JOIN ("T1" NATURAL, "T2" INDEX ("PUBLIC"."IT"))
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
