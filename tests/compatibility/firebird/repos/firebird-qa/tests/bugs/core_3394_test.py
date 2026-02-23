#coding:utf-8

"""
ID:          issue-3760
ISSUE:       3760
TITLE:       Failed attempt to violate unique constraint could leave unneeded "lock conflict" error in status-vector
DESCRIPTION:
JIRA:        CORE-3394
FBTEST:      bugs.core_3394
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t(id int, constraint t_pk primary key(id) using index t_id);
    commit;
    SET TRANSACTION READ COMMITTED RECORD_VERSION NO WAIT;
    set term ^;
    execute block as
    begin
      insert into t values(1);
      in autonomous transaction do
      insert into t values(1);
    end
    ^
    set term ;^
    rollback;
"""

act = isql_act('db', test_script, substitutions=[('(-)?At block line: [\\d]+, col: [\\d]+', 'At block line')])

expected_out_5x = """
    Statement failed, SQLSTATE = 23000
    violation of PRIMARY or UNIQUE KEY constraint "T_PK" on table "T"
    -Problematic key value is ("ID" = 1)
    At block line
"""

expected_out_6x = """
    Statement failed, SQLSTATE = 23000
    violation of PRIMARY or UNIQUE KEY constraint "T_PK" on table "PUBLIC"."T"
    -Problematic key value is ("ID" = 1)
    At block line
"""

@pytest.mark.version('>=3.0')
def test_2(act: Action):
    act.expected_stdout = expected_out_5x if act.is_version('<6') else expected_out_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

