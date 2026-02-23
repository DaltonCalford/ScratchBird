#coding:utf-8

"""
ID:          issue-2489
ISSUE:       2489
TITLE:       Computed expressions may be optimized badly if used inside the RETURNING clause of the INSERT statement
DESCRIPTION:
JIRA:        CORE-2053
FBTEST:      bugs.core_2053
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
    set list on;
    create table t1 (col1 int);
    create index i1 on t1 (col1);
    commit;
    insert into t1 (col1) values (1);
    commit;
    create table t2 (col2 int);
    commit;

    SET PLAN ON;
    insert into t2 (col2) values (1) returning case when exists (select 1 from t1 where col1 = col2) then 1 else 0 end as insert_outcome;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (T1 INDEX (I1))
    INSERT_OUTCOME 1
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."T1" INDEX ("PUBLIC"."I1"))
    INSERT_OUTCOME 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
