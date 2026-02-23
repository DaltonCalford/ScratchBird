#coding:utf-8

"""
ID:          issue-2808
ISSUE:       2808
TITLE:       ALTER VIEW could remove column used in stored procedure or trigger
DESCRIPTION:
JIRA:        CORE-2386
FBTEST:      bugs.core_2386
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
"""

db = db_factory(init=init_script)

test_script = """
    set term ^ ;
    create view v_test (f1, f2) as select 1, 2 from rdb$database
    ^
    create procedure sp_test as
        declare i int;
    begin
        select f1, f2 from v_test into :i, :i;
    end
    ^
    commit
    ^
    alter view v_test (f1) as select 1 from rdb$database
    ^

"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN V_TEST.F2
    -there are 1 dependencies
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN "PUBLIC"."V_TEST"."F2"
    -there are 1 dependencies
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
