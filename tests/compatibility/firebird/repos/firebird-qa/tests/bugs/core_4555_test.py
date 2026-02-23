#coding:utf-8

"""
ID:          issue-4873
ISSUE:       4873
TITLE:       DDL trigger remains active after dropped
DESCRIPTION:
JIRA:        CORE-4555
FBTEST:      bugs.core_4555
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
    create exception ddl_exception 'You have no right to create exceptions. Learn DDL triggers first!';
    commit;
    set term ^;
    create or alter trigger t_ddl
    active before create exception
    as
    begin
      if (current_user <> 'DUDE') then
        exception ddl_exception;
    end
    ^
    set term ;^
    commit;

    create exception user_exception 'Invalid remainder found for case-1.';
    commit;

    drop trigger t_ddl;
    commit;

    create exception user_exception 'Invalid remainder found for case-2.'
    ;
    commit;

    set list on;
    set count on;
    select rdb$exception_name, rdb$message
    from rdb$exceptions
    order by rdb$exception_name;
"""

act = isql_act('db', test_script, substitutions=[('=.*', ''), ('line:\\s[0-9]+,', 'line: x'),
                                                 ('col:\\s[0-9]+', 'col: y')])

expected_stdout_5x = """
    Statement failed, SQLSTATE
    unsuccessful metadata update
    -CREATE EXCEPTION USER_EXCEPTION failed
    -exception 1
    -DDL_EXCEPTION
    -You have no right to create exceptions. Learn DDL triggers first!
    -At trigger 'T_DDL' line: x col: y
    RDB$EXCEPTION_NAME              DDL_EXCEPTION
    RDB$MESSAGE                     You have no right to create exceptions. Learn DDL triggers first!
    RDB$EXCEPTION_NAME              USER_EXCEPTION
    RDB$MESSAGE                     Invalid remainder found for case-2.
    Records affected: 2
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE
    unsuccessful metadata update
    -CREATE EXCEPTION "PUBLIC"."USER_EXCEPTION" failed
    -exception 1
    -"PUBLIC"."DDL_EXCEPTION"
    -You have no right to create exceptions. Learn DDL triggers first!
    -At trigger "PUBLIC"."T_DDL" line: x col: y
    RDB$EXCEPTION_NAME              DDL_EXCEPTION
    RDB$MESSAGE                     You have no right to create exceptions. Learn DDL triggers first!
    RDB$EXCEPTION_NAME              USER_EXCEPTION
    RDB$MESSAGE                     Invalid remainder found for case-2.
    Records affected: 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
