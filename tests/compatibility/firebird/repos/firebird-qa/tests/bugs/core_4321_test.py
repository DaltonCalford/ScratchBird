#coding:utf-8

"""
ID:          issue-4644
ISSUE:       4644
TITLE:       Regression: ISQL does not destroy the SQL statement
DESCRIPTION:
JIRA:        CORE-4321
NOTES:
    [31.12.2024] pzotov
    Added forgotten semicolon after 'SET LIST ON'.
    Parsing problem appeared on 6.0.0.0.570 after d6ad19aa07deeaac8107a25a9243c5699a3c4ea1
    ("Refactor ISQL creating FrontendParser class").
    It looks weird how it could work w/o 'token unknown / list' all this time in all major FB versions :-)
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- NB: 2.1.7 FAILED, output contains '4' for select count(*) ...
    set list on;
    select /* tag_for_watch */ 1 point_a from rdb$database;
    select /* tag_for_watch */ 1 point_a from rdb$database;
    select /* tag_for_watch */ 1 point_a from rdb$database;
    select /* tag_for_watch */ 1 point_a from rdb$database;

    select count(*) count_after_point_a from mon$statements s
    where s.mon$sql_text containing '/* tag_for_watch */'
    ;
    commit;
    select count(*) count_after_commit_a from mon$statements s
    where s.mon$sql_text containing '/* tag_for_watch */'
    ;

    select /* tag_for_watch */ 1 point_b from rdb$database;
    select /* tag_for_watch */ 1 point_b from rdb$database;
    select /* tag_for_watch */ 1 point_b from rdb$database;
    select /* tag_for_watch */ 1 point_b from rdb$database;

    select count(*) count_after_point_b from mon$statements s
    where s.mon$sql_text containing '/* tag_for_watch */'
    ;
    commit;

    select count(*) count_after_commit_b from mon$statements s
    where s.mon$sql_text containing '/* tag_for_watch */'
    ;
"""

act = isql_act('db', test_script,  substitutions=[('[ \t]+', ' ')] )

expected_stdout = """
    POINT_A 1
    POINT_A 1
    POINT_A 1
    POINT_A 1
    COUNT_AFTER_POINT_A 1
    COUNT_AFTER_COMMIT_A 1
    POINT_B 1
    POINT_B 1
    POINT_B 1
    POINT_B 1
    COUNT_AFTER_POINT_B 1
    COUNT_AFTER_COMMIT_B 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

