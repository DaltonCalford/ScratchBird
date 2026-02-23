#coding:utf-8

"""
ID:          issue-6133
ISSUE:       6133
TITLE:       Provide name of read-only column incorrectly referenced in UPDATE ... SET xxx
DESCRIPTION:
  Table with computed field (non-ascii) that is result of addition is used here.
  UPDATE statement is used in trivial form, then as 'update or insert' and as 'merge'.
  All cases should produce STDERR with specifying table name and R/O column after dot.
JIRA:        CORE-5874
FBTEST:      bugs.core_5874
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    recreate table test(id int, x int, y int, "hozzáadása" computed by (x * y) );
    commit;

    set planonly;

    update test set "hozzáadása" = 1;

    update or insert into test(id, "hozzáadása")
    values(1, 111) matching(id)
    returning "hozzáadása";

    merge into test t
    using( select 1 as id, 2 as x, 3 as y from rdb$database ) s on s.id = t.id
    when matched then
        update set "hozzáadása" = 1
    ;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.hozzáadása

    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.hozzáadása

    Statement failed, SQLSTATE = 42000
    attempted update of read-only column TEST.hozzáadása
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "PUBLIC"."TEST"."hozzáadása"

    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "PUBLIC"."TEST"."hozzáadása"

    Statement failed, SQLSTATE = 42000
    attempted update of read-only column "PUBLIC"."TEST"."hozzáadása"
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
