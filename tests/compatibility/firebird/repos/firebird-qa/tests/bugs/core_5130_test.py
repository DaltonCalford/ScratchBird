#coding:utf-8

"""
ID:          issue-5414
ISSUE:       5414
TITLE:       Compiler issues message about "invalid request BLR" when attempt to compile
  wrong DDL of view with both subquery and "WITH CHECK OPTION" in its DDL
DESCRIPTION:
JIRA:        CORE-5130
FBTEST:      bugs.core_5130
NOTES:
    [01.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed proper result on:  WI-V3.0.0.32380
    create or alter view v1 as select 1 id from rdb$database;
    recreate table t1(id int, x int, y int);
    commit;

    alter view v1 as
    select * from t1 a
    where
        not exists(select * from t1 r where r.x > a.x)
    with check option
    ;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    VIEW_NAME = 'V1' if act.is_version('<6') else '"V1"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER VIEW {SQL_SCHEMA_PREFIX}{VIEW_NAME} failed
        -Dynamic SQL Error
        -SQL error code = -607
        -No subqueries permitted for VIEW WITH CHECK OPTION
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
