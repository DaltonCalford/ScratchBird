#coding:utf-8

"""
ID:          issue-4430
ISSUE:       4430
TITLE:       Bad optimization of OR predicates applied to unions
DESCRIPTION:
JIRA:        CORE-4102
FBTEST:      bugs.core_4102
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
    SET PLANONLY;
    select * from
    (
      select rdb$relation_id as id
        from rdb$relations r
      union all
      select rdb$relation_id as id
        from rdb$relations r
    ) x
    where x.id = 0 or x.id = 1;
"""

act = isql_act('db', test_script)

substitutions = [(r'RDB\$INDEX_\d+', 'RDB$INDEX_*')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (X R INDEX (RDB$INDEX_*, RDB$INDEX_*), X R INDEX (RDB$INDEX_*, RDB$INDEX_*))
"""
expected_stdout_6x = """
    PLAN ("X" "R" INDEX ("SYSTEM"."RDB$INDEX_*", "SYSTEM"."RDB$INDEX_*"), "X" "R" INDEX ("SYSTEM"."RDB$INDEX_*", "SYSTEM"."RDB$INDEX_*"))
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
