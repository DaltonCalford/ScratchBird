#coding:utf-8

"""
ID:          issue-5073
ISSUE:       5073
TITLE:       Table aliasing is unnecessary required when doing UPDATE ... RETURNING RDB$ pseudo-columns
DESCRIPTION:
JIRA:        CORE-4774
FBTEST:      bugs.core_4774
NOTES:
    After fix #6815 execution plan contains 'Local_Table' (FB 5.0+) for DML with RETURNING clauses:
    "When such a statement is executed, Firebird should execute the statement to completion
    and collect all requested data in a type of temporary table, once execution is complete,
    fetches are done against this temporary table"

    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t(id int, x int);
    commit;
    insert into t values(1, 100);
    commit;
    set planonly;
    insert into t(id, x) values(2, 200) returning rdb$db_key;
    delete from t where id=1 returning rdb$db_key;
    update t set x=-x where id=2 returning rdb$db_key;
    update t set x=-x where id=2 returning rdb$record_version;
"""

act = isql_act('db', test_script)

expected_stdout_4x = """
    PLAN (T NATURAL)
    PLAN (T NATURAL)
    PLAN (T NATURAL)
"""

expected_stdout_5x = """
    PLAN (T NATURAL)
    PLAN (Local_Table NATURAL)
    PLAN (T NATURAL)
    PLAN (Local_Table NATURAL)
    PLAN (T NATURAL)
    PLAN (Local_Table NATURAL)
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."T" NATURAL)
    PLAN (Local_Table NATURAL)
    PLAN ("PUBLIC"."T" NATURAL)
    PLAN (Local_Table NATURAL)
    PLAN ("PUBLIC"."T" NATURAL)
    PLAN (Local_Table NATURAL)
"""

@pytest.mark.version('>=3.0')
def test(act: Action):
    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
