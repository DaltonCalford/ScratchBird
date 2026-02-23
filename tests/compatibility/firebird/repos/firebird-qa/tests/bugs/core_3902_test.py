#coding:utf-8

"""
ID:          issue-4238
ISSUE:       4238
TITLE:       Derived fields may not be optimized via an index
DESCRIPTION:
JIRA:        CORE-3902
FBTEST:      bugs.core_3902
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SET PLANONLY;
select rdb$database.rdb$relation_id from rdb$database
  left outer join
  ( select rdb$relations.rdb$relation_id as tempid
    from rdb$relations ) temp (tempid)
  on temp.tempid = rdb$database.rdb$relation_id;
select rdb$database.rdb$relation_id from rdb$database
  left outer join
  ( select rdb$relations.rdb$relation_id
    from rdb$relations ) temp
  on temp.rdb$relation_id = rdb$database.rdb$relation_id;

"""

substitutions = [(r'RDB\$INDEX_\d+', 'RDB$INDEX_*')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN JOIN (RDB$DATABASE NATURAL, TEMP RDB$RELATIONS INDEX (RDB$INDEX_1))
    PLAN JOIN (RDB$DATABASE NATURAL, TEMP RDB$RELATIONS INDEX (RDB$INDEX_1))
"""
expected_stdout_6x = """
    PLAN JOIN ("SYSTEM"."RDB$DATABASE" NATURAL, "TEMP" "SYSTEM"."RDB$RELATIONS" INDEX ("SYSTEM"."RDB$INDEX_1"))
    PLAN JOIN ("SYSTEM"."RDB$DATABASE" NATURAL, "TEMP" "SYSTEM"."RDB$RELATIONS" INDEX ("SYSTEM"."RDB$INDEX_1"))
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
