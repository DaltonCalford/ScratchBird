#coding:utf-8

"""
ID:          issue-3160
ISSUE:       3160
TITLE:       ALTERING OR DROPPING PROCEDURE which has type of domain parameter leads to attempt to delete that domain
DESCRIPTION:
JIRA:        CORE-2768
FBTEST:      bugs.core_2768
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v_fields as
    select rf.rdb$field_name f_name, rf.rdb$field_length f_length, rf.rdb$field_type f_type
    from rdb$database r
    left join rdb$fields rf on rf.rdb$field_name = upper('dm_test')
    ;

    recreate view v_dependencies as
    select rd.rdb$dependent_name as dep_obj_name, rd.rdb$depended_on_name as dep_on_what
    from rdb$database r
    left join rdb$dependencies rd on rd.rdb$dependent_name = upper('sp_test')
    ;

    create or alter procedure sp_test as begin end
    ;
    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop domain dm_test';
            when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create domain dm_test int;
    commit;
    create or alter procedure sp_test( a_01 type of dm_test ) as begin end;
    commit;

    set count on;
    set list on;

    select * from v_fields ;
    select * from v_dependencies ;
    commit;

    alter procedure sp_test as begin end;
    commit; -- here was error in 2.5 Beta 2, 2.5 RC1

    select * from v_fields ;
    select * from v_dependencies ;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    F_NAME                          DM_TEST
    F_LENGTH                        4
    F_TYPE                          8


    Records affected: 1

    DEP_OBJ_NAME                    SP_TEST
    DEP_ON_WHAT                     DM_TEST


    Records affected: 1

    F_NAME                          DM_TEST
    F_LENGTH                        4
    F_TYPE                          8


    Records affected: 1

    DEP_OBJ_NAME                    <null>
    DEP_ON_WHAT                     <null>


    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

