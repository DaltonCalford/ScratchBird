#coding:utf-8

"""
ID:          issue-3464
ISSUE:       3464
TITLE:       Add clause ALTER DOMAIN <name> {DROP | SET} NOT NULL
DESCRIPTION:
JIRA:        CORE-3085
FBTEST:      bugs.core_3085
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
    create domain dm_int int;
    commit;
    create table test(x dm_int);
    commit;

    set term ^;
    create procedure sp_test(a dm_int) returns(msg varchar(30)) as
    begin
        msg='intro proc sp_test: a=' || coalesce(a, 'null');
        suspend;
    end
    ^
    set term ;^
    commit;

    insert into test values(1);
    insert into test values(2);
    insert into test values(3);
    commit;

    set list on;

    alter domain dm_int set not null;
    commit;

    select msg from sp_test(null);
    update test set x=null where x=2;
    commit;

    alter domain dm_int drop not null;
    commit;

    select msg from sp_test(null);
    update test set x=null where x=2 returning x;
    commit;

    alter domain dm_int set not null;
"""

substitutions = [ ('[ \t]+', ' ') ] 
act = isql_act('db', test_script, substitutions = substitutions)


expected_out_5x = """
    Statement failed, SQLSTATE = 42000
    validation error for variable A, value "*** null ***"
    -At procedure 'SP_TEST'

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X", value "*** null ***"
    MSG                             intro proc sp_test: a=null
    X                               <null>

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X of table TEST NOT NULL because there are NULLs present
"""

expected_out_6x = """
    Statement failed, SQLSTATE = 42000
    validation error for variable "A", value "*** null ***"
    -At procedure "PUBLIC"."SP_TEST"

    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST"."X", value "*** null ***"
    MSG                             intro proc sp_test: a=null
    X                               <null>

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field "X" of table "PUBLIC"."TEST" NOT NULL because there are NULLs present
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_out_5x if act.is_version('<6') else expected_out_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
