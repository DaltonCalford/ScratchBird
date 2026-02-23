#coding:utf-8

"""
ID:          issue-4576
ISSUE:       4576
TITLE:       Add table name to text of validation contraint error message, to help identify error context
DESCRIPTION:
JIRA:        CORE-4252
FBTEST:      bugs.core_4252
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter procedure sp_test(a_arg smallint) as begin end;
    commit;

    recreate table t1(x int not null );
    recreate table "T2"("X" int not null );
    commit;

    set term ^;
    create or alter procedure sp_test(a_arg smallint) as
    begin
      if  ( a_arg = 1 ) then insert into t1(x) values(null);
      else insert into "T2"("X") values(null);
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    execute procedure sp_test(1);
    execute procedure sp_test(2);
"""


substitutions = [('line(:)?\\s+.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 23000
    validation error for column "T1"."X", value "*** null ***"
    -At procedure 'SP_TEST'
    
    Statement failed, SQLSTATE = 23000
    validation error for column "T2"."X", value "*** null ***"
    -At procedure 'SP_TEST'
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."T1"."X", value "*** null ***"
    -At procedure "PUBLIC"."SP_TEST"

    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."T2"."X", value "*** null ***"
    -At procedure "PUBLIC"."SP_TEST"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
