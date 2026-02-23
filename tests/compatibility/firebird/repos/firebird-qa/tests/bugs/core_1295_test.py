#coding:utf-8

"""
ID:          issue-1716
ISSUE:       1716
TITLE:       Bad optimization of queries with DB_KEY
DESCRIPTION:
JIRA:        CORE-1295
FBTEST:      bugs.core_1295
NOTES:
    [24.06.2025] pzotov
    Separated execution plans for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema name and quotes to enclosing object names.
    Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.858; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    SET PLANONLY;
    select * from rdb$relations where rdb$db_key = ? and rdb$relation_id = 0;
    select * from rdb$relations where rdb$db_key = ? and rdb$relation_name = 'RDB$RELATIONS';
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (RDB$RELATIONS INDEX ())
    PLAN (RDB$RELATIONS INDEX ())
"""

expected_stdout_6x = """
    PLAN ("SYSTEM"."RDB$RELATIONS" INDEX ())
    PLAN ("SYSTEM"."RDB$RELATIONS" INDEX ())
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
