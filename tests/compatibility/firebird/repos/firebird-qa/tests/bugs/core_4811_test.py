#coding:utf-8

"""
ID:          issue-5109
ISSUE:       5109
TITLE:       Make user names behave according to SQL identifiers rules
DESCRIPTION:
JIRA:        CORE-4811
FBTEST:      bugs.core_4811
NOTES:
    [29.06.2025] pzotov
    Re-implemented: use f-notation and variables to be substituted in the expected output.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name='tmp$c4811', password='1')
tmp_role = role_factory('db', name='Boss')

substitutions = [('set echo.*', ''), ('[ \t]+', ' '), ('exception \\d+', 'exception'), ('line(:)?\\s+\\d+.*', '')]
act = isql_act('db', substitutions = substitutions)


@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User, tmp_role: Role):

    test_script = f"""
        set wng off;
        set list on;
        create or alter procedure sp_check_actual_role as begin end;
        commit;

        recreate exception ex_have_no_role 'You''ve specified role: >@1< -- but your actual role is NONE.';

        set term ^;
        create or alter procedure sp_check_actual_role(
            a_probe_role varchar(31)
        ) returns(
            checking varchar(80),
            result varchar(31)
        ) as
        begin
            if ( upper(current_role) = 'NONE' )
            then
                exception ex_have_no_role using ( a_probe_role );

            checking = 'role: >' || a_probe_role || '< - '
                       || trim(
                               iif( a_probe_role containing '''', 'in apostrophes',
                                   iif( a_probe_role containing '"', 'in double quotes', 'without delimiters' )
                                  )
                              )
                       || ', ' || iif( upper(a_probe_role) = a_probe_role, 'UPPER case', 'CaMeL case' )
            ;
            result = current_role;
            suspend;
        end
        ^
        set term ;^
        commit;

        set bail on;
        set echo on;
        grant Boss to Tmp$c4811;
        grant usage on exception ex_have_no_role to Tmp$c4811;
        grant execute on procedure sp_check_actual_role to Tmp$c4811;
        set echo off;
        set bail off;
        -- show grants;
        commit;

        -- set echo on;

        -- checking for USER name:

        connect '{act.db.dsn}' user 'Tmp$c4811' password '1';
        -- PASSES since http://sourceforge.net/p/firebird/code/62016 (2015-07-16 14:26), this was build = 31981
        select 'user: >''Tmp$c4811''< - in apostrophes, CaMeL case' checking, current_user as result from rdb$database;
        commit;

        connect '{act.db.dsn}' user 'TMP$C4811' password '1'; -- should PASS, checked on builds 31948, 31981
        select 'user: >''TMP$C4811''< - in apostrophes, UPPER case' checking, current_user as result from rdb$database;
        commit;

        connect '{act.db.dsn}' user Tmp$c4811 password '1'; -- should PASS, checked on builds 31948, 31981
        select 'user: >Tmp$c4811< - without delimiters, CaMeL case' checking, current_user as result from rdb$database;
        commit;

        connect '{act.db.dsn}' user TMP$C4811 password '1'; -- should PASS, checked on builds 31948, 31981
        select 'user: >TMP$C4811< - without delimiters, UPPER case' checking, current_user as result from rdb$database;
        commit;

        connect '{act.db.dsn}' user "Tmp$c4811" password '1'; -- should *** FAIL ***
        select 'user: >"Tmp$c4811"< - in double quotes, CaMeL case' checking, current_user as result from rdb$database;
        commit;

        connect '{act.db.dsn}' user "TMP$C4811" password '1'; -- should PASS, checked on builds 31948, 31981
        select 'user: >"TMP$C4811" - in double quotes, UPPER case' checking, current_user as result from rdb$database;
        commit;

        -- checking for ROLE (actual role in all following cases will be: [BOSS], checked on builds 31948, 31981)

        -- Statement that created role (see above):
        -- create role Boss;

        -- Enclosing role in apostrophes and specifying it exactly like it was in its creation sttm:
        connect '{act.db.dsn}' user 'TMP$C4811' password '1' role 'Boss';
        select * from sp_check_actual_role( '''Boss''' ); --------------- should return: BOSS
        commit;

        -- Enclosing role in apostrophes and specifying it in UPPERCASE (i.e. differ than in its CREATE ROLE statement):
        connect '{act.db.dsn}' user 'TMP$C4811' password '1' role 'BOSS';
        select * from sp_check_actual_role( '''BOSS''' ); --------------- should return: BOSS
        commit;

        -- do NOT enclosing role in any delimiters and change CaSe of its characters (i.e. differ than in its CREATE ROLE statement):
        connect '{act.db.dsn}' user 'TMP$C4811' password '1' role BosS;
        select * from sp_check_actual_role( 'BosS' );     --------------- should return: BOSS
        commit;

        -- do NOT enclosing role in any delimiters and specifying it in UPPERCASE (i.e. differ than in its CREATE ROLE statement):
        connect '{act.db.dsn}' user 'TMP$C4811' password '1' role BOSS;
        select * from sp_check_actual_role( 'BOSS' );      --------------- should return: BOSS
        commit;

        -- Enclosing role in double quotes and change CaSe of its characters (i.e. differ than in its CREATE ROLE statement):
        connect '{act.db.dsn}' user 'TMP$C4811' password '1' role "BoSs";
        select * from sp_check_actual_role( '"BoSs"' );    --------------- should raise EX_HAVE_NO_ROLE, actual role will be 'NONE'
        commit;

        -- Enclosing role in double quotes and specifying it in UPPERCASE (i.e. differ than in its CREATE ROLE statement):
        connect '{act.db.dsn}' user 'TMP$C4811' password '1' role "BOSS";
        select * from sp_check_actual_role( '"BOSS"' );    --------------- should return: BOSS
        commit;
    """

    
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    EXCEPTION_NAME = "EX_HAVE_NO_ROLE" if act.is_version('<6') else  '"EX_HAVE_NO_ROLE"'
    STORED_PROC_NAME = "'SP_CHECK_ACTUAL_ROLE'" if act.is_version('<6') else  '"SP_CHECK_ACTUAL_ROLE"'
    expected_stdout = f"""
        grant Boss to Tmp$c4811;
        grant usage on exception ex_have_no_role to Tmp$c4811;
        grant execute on procedure sp_check_actual_role to Tmp$c4811;
        CHECKING                        user: >'Tmp$c4811'< - in apostrophes, CaMeL case
        RESULT                          {tmp_user.name.upper()}
        CHECKING                        user: >'TMP$C4811'< - in apostrophes, UPPER case
        RESULT                          {tmp_user.name.upper()}
        CHECKING                        user: >Tmp$c4811< - without delimiters, CaMeL case
        RESULT                          {tmp_user.name.upper()}
        CHECKING                        user: >TMP$C4811< - without delimiters, UPPER case
        RESULT                          {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
        CHECKING                        user: >"TMP$C4811" - in double quotes, UPPER case
        RESULT                          {tmp_user.name.upper()}
        CHECKING                        role: >'Boss'< - in apostrophes, CaMeL case
        RESULT                          {tmp_role.name.upper()}
        CHECKING                        role: >'BOSS'< - in apostrophes, UPPER case
        RESULT                          {tmp_role.name.upper()}
        CHECKING                        role: >BosS< - without delimiters, CaMeL case
        RESULT                          {tmp_role.name.upper()}
        CHECKING                        role: >BOSS< - without delimiters, UPPER case
        RESULT                          {tmp_role.name.upper()}
        Statement failed, SQLSTATE = HY000
        exception
        -{SQL_SCHEMA_PREFIX}{EXCEPTION_NAME}
        -You've specified role: >"BoSs"< -- but your actual role is NONE.
        -At procedure {SQL_SCHEMA_PREFIX}{STORED_PROC_NAME}
        CHECKING                        role: >"BOSS"< - in double quotes, UPPER case
        RESULT                          {tmp_role.name.upper()}
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
