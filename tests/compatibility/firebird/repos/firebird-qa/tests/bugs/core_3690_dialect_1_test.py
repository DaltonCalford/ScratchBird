#coding:utf-8

"""
ID:          issue-4038
ISSUE:       4038
TITLE:       Wrong warning message for ambiguous query
DESCRIPTION: SQL dialect 1 allows such queries for backward compatibility reasons
JIRA:        CORE-3690
FBTEST:      bugs.core_3690
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    set list on;
    select m.mon$sql_dialect from mon$database m;
    select 0*rdb$relation_id as id from rdb$database,rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

expected_stdout_5x = """
    MON$SQL_DIALECT                 1
    SQL warning code = 204
    -Ambiguous field name between table RDB$DATABASE and table RDB$DATABASE
    -RDB$RELATION_ID
    ID                              0
"""

expected_stdout_6x = """
    MON$SQL_DIALECT                 1
    SQL warning code = 204
    -Ambiguous field name between table "SYSTEM"."RDB$DATABASE" and table "SYSTEM"."RDB$DATABASE"
    -RDB$RELATION_ID
    ID                              0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=['-q', '-sql_dialect', '1'], input=test_script, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
