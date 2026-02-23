#coding:utf-8

"""
ID:          issue-3188
ISSUE:       3188
TITLE:       Incomplete plan output (lack of view names) when selecting from views containing procedures inside
DESCRIPTION:
JIRA:        CORE-2798
FBTEST:      bugs.core_2798
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
    create table t1 (col int);
    commit;

    set term ^;
    create procedure p1 returns (res int) as begin suspend; end^
    set term ;^
    commit;

    create view v as select 1 as num from t1, t1 as t2, p1, p1 as p2;
    commit;
    set plan on;
    select * from v;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN JOIN (V P1 NATURAL, V P2 NATURAL, V T1 NATURAL, V T2 NATURAL) 
"""

expected_stdout_6x = """
    PLAN JOIN ("PUBLIC"."V" "PUBLIC"."P1" NATURAL, "PUBLIC"."V" "P2" NATURAL, "PUBLIC"."V" "PUBLIC"."T1" NATURAL, "PUBLIC"."V" "T2" NATURAL)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
