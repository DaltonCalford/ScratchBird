#coding:utf-8

"""
ID:          issue-4539
ISSUE:       4539
TITLE:       GTT should not reference permanent relation
DESCRIPTION:
JIRA:        CORE-4214
FBTEST:      bugs.core_4214
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    commit;
    create global temporary table gtt_main(x int, y int, constraint gtt_main_unq unique(x,y) using index gtt_main_unq);
    create table fix_main(x int, y int, constraint fix_main_unq unique(x,y) using index fix_main_unq);
    create global temporary table gtt_detl(x int, y int,
      constraint gtt_detl_fk_to_gtt foreign key(x,y) references gtt_main(x, y),
      constraint gtt_detl_fk_to_fix foreign key(x,y) references fix_main(x, y)
    );
    create table fix_detl(x int, y int,
      constraint fix_detl_fk_to_fix foreign key(x,y) references fix_main(x, y),
      constraint fix_detl_fk_to_gtt foreign key(x,y) references gtt_main(x, y)
    );
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = HY000
    unsuccessful metadata update
    -CREATE TABLE GTT_DETL failed
    -global temporary table "GTT_DETL" of type ON COMMIT DELETE ROWS cannot reference persistent table "FIX_MAIN"

    Statement failed, SQLSTATE = HY000
    unsuccessful metadata update
    -CREATE TABLE FIX_DETL failed
    -persistent table "FIX_DETL" cannot reference global temporary table "GTT_MAIN" of type ON COMMIT DELETE ROWS
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = HY000
    unsuccessful metadata update
    -CREATE TABLE "PUBLIC"."GTT_DETL" failed
    -global temporary table "PUBLIC"."GTT_DETL" of type ON COMMIT DELETE ROWS cannot reference persistent table "PUBLIC"."FIX_MAIN"

    Statement failed, SQLSTATE = HY000
    unsuccessful metadata update
    -CREATE TABLE "PUBLIC"."FIX_DETL" failed
    -persistent table "PUBLIC"."FIX_DETL" cannot reference global temporary table "PUBLIC"."GTT_MAIN" of type ON COMMIT DELETE ROWS
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
