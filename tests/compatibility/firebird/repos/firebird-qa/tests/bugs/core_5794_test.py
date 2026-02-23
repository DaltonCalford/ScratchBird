#coding:utf-8

"""
ID:          issue-6057
ISSUE:       6057
TITLE:       Wrong behavour FOR <select_stmt> [AS CURSOR cursorname] with next update and delete
DESCRIPTION:
  Despite that this ticket was closed with solution "Won't fix" i see that it is useful for additional
  checking of cursor stability feature (new in 3.0).
  See also:
  1) test for #3728 (CORE-3362)
  2) comment in this ticket (CORE-5794) by hvlad:
  "Since Firebird3 <..> cursor doesn't see the changes made by "inner" statements."
JIRA:        CORE-5794
FBTEST:      bugs.core_5794
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test_table (
        id integer,
        val integer
    );
    commit;
    insert into test_table (id, val) values (1, 10);
    commit;

    recreate exception test_exception 'test';

    set term ^ ;
    create or alter trigger test_table_bd for test_table active before delete position 0 as
    begin
        rdb$set_context('USER_SESSION','TRIGGER_OLD_ID', coalesce(old.id,'null') );
        rdb$set_context('USER_SESSION','TRIGGER_OLD_VAL', coalesce(old.val,'null') );
        if (old.val >0 ) then
            exception test_exception 'it is forbidden to delete row with val>0 (id = '||coalesce(old.id, 'null')||', val='||coalesce(old.val,'null')||')';
    end
    ^

    execute block as
        declare curVal integer;
        declare curID integer;
    begin
        for select id, val
        from test_table
        where val>0
        into curid, curval
        as cursor tmpcursor
        do begin
            update test_table
            set val=0
            where current of tmpcursor;

            -- NO error in 2.5:
            delete from test_table
            where current of tmpcursor;
        end
    end
    ^
    set term ;^

    set list on;
    select mon$variable_name as ctx_var, mon$variable_value as ctx_value from mon$context_variables;
"""

substitutions = [ ('[ \t]+', ' '), ( r'line(:)?\s+\d+.*', '' ) ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -TEST_EXCEPTION
    -it is forbidden to delete row with val>0 (id = 1, val=10)
    -At trigger 'TEST_TABLE_BD'
    At block

    CTX_VAR TRIGGER_OLD_ID
    CTX_VALUE 1

    CTX_VAR TRIGGER_OLD_VAL
    CTX_VALUE 10
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."TEST_EXCEPTION"
    -it is forbidden to delete row with val>0 (id = 1, val=10)
    -At trigger "PUBLIC"."TEST_TABLE_BD"
    At block

    CTX_VAR TRIGGER_OLD_ID
    CTX_VALUE 1

    CTX_VAR TRIGGER_OLD_VAL
    CTX_VALUE 10
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
