#coding:utf-8

"""
ID:          issue-6935
ISSUE:       6935
TITLE:       SQL SECURITY DEFINER has inconsistent behaviour if the object owner is non-privileged
DESCRIPTION:
  We create three users:
    *  'tmp$sp_definer' - he will be temporary granted to create procedures
       and will create several SPs with 'SQL SECURITY DEFINER' modifier.
       These procedures are following:
         1 'sp_chk_mon_access' - it tries to gather all avaliable data
           about user attachments and active statements which they perform
           (i.e. it queries ALL data from mon$-tables);
         2 'sp_clear_ext_pool' - it makes attempt to clear external connections pool
           (it does this within begin...when any...end block);
         3 'sp_chk_sql_rights' - it tries to insert record in the table 'log_table'.
       NOTE. After these procedures will be created, test does re-connect as SYSDBA
       and revokes any privilege from 'tmp$sp_definer', i.e. it becomes non-privileged.
    * 'tmp$outside_caller' - he will execute procedure 'sp_chk_syspriv_main' which can
       be invoked by public and calls, in turn, procedure 'sp_chk_mon_access'.
    * 'tmp$almost_dba' - he will be granted with several system privileged:
      MONITOR_ANY_ATTACHMENT, MODIFY_EXT_CONN_POOL and ACCESS_ANY_OBJECT_IN_DATABASE.

  Following actions must be done after this:
    * 1) check ticket issue:
          -----------
          "1. Usage of MON$ tables
           ... when user 'tmp$outside_caller' calls procedure with definer 'tmp$sp_definer'
           which (ooops) sees attachments by user 'tmp$outside_caller'
          -----------
      In order to do that, we connect as 'tmp$sp_definer' and invoke SP 'sp_chk_mon_access'
      using ES mechanism and pass user 'tmp$outside_caller' access rights there
      (NB: this SP was created by 'tmp$sp_definer' and has 'SQL SECURITY DEFINER' modifier).

      Inside SP effective user becomes 'tmp$sp_definer' and he is also non-privileged (as 'tmp$outside_caller').
      This means that he must be able to see only mon$attachments rows where mon$user = 'tmp$sp_definer'.
      He must NOT see rows where mon$user = 'tmp$outside_caller' (which was so before this ticket fixed).
    * 2) check ticket issue:
          -----------
          "2. System privileges.
           if *privileged* user 'tmp$almost_dba' calls procedure with non-privileged definer 'tmp$sp_definer', it still
           can perform every DBA action inside that SP, like if it was executed under 'tmp$almost_dba's permissions"
          -----------
      Three system privileges are checked here:
          a. MONITOR_ANY_ATTACHMENT
              We connect as 'tmp$almost_dba' and invoke SP 'sp_chk_mon_access' as (again) user 'tmp$outside_caller'.
              Inside SP effective user becomes 'tmp$sp_definer'.
              He must be able to see only mon$attachments rows with mon$user = 'tmp$sp_definer' - but there are no such
              rows in monattachments at this moment because we did not connect as 'tmp$sp_definer'.
              This means that 'sp_chk_mon_access' must return NULLs instead of non-empty data from mon$ tables.
          b. MODIFY_EXT_CONN_POOL
              connect as 'tmp$almost_dba' and invoke SP 'sp_clear_ext_pool' which tries to clear Ext. Connections Pool (ECP).
              But effective user inside this SP becomes 'tmp$sp_definer' and he has no any system privilege.
              This means that attempt to clear ECP must fail.

          c. ACCESS_ANY_OBJECT_IN_DATABASE
              connect as 'tmp$almost_dba' and invoke SP 'sp_chk_sql_rights' which tries to make DML agains table 'log_table'.
              Effective user inside this SP becomes 'tmp$sp_definer' and he has no any system privilege.
              Consequently, attempt to insert row into this table must fail.
NOTES:
[01.09.2021]
  Added "where"-filtering for exclude statement with query to RDB$AUTH_MAPPING (4.0 Classic).
FBTEST:      bugs.gh_6935

NOTES:
    [01.12.2023] pzotov
    New behaviour of ISQL was introduced after implementation of PR #7868: SET AUTOTERM.
    Since that was implemented, ISQL handles comments (single- and multi-lined) as PART of statement that follows these comments.
    In other words, ISQL in 6.x does not 'swallow' comments and sends them to engine together with statement that follows.
    This means that such 'pair' (comment PLUS statement) can be 'unexpectedly' seen in MON$STATEMENTS.MON$SQL_TEXT.

    Currently this is not considered as a bug, see note by Adriano: https://groups.google.com/g/firebird-devel/c/AM8vlA3YJws
    Because of this, we have (in this test) to either not use comments at all or filter them out by applying substitution which
    will 'know' about some special text ('comment_tag') that must be suppressed.

    Checked on 6.0.0.163, 5.0.0.1284, 4.0.5.3033
"""

import pytest
from firebird.qa import *

db = db_factory()

COMMENT_TAG='DONT_SHOW_IN_OUTPUT'
test_script = f"""
    set list on;
    set bail on;
    create table log_table(id int, eff_user varchar(32), mon_user varchar(32) );

    create or alter user tmp$sp_definer password '123';
    create or alter user tmp$outside_caller password '123';
    create or alter user tmp$almost_dba password '123';
    commit;

    grant create procedure to tmp$sp_definer;
    commit;

    -- {COMMENT_TAG} ###################################################
    connect  '$(DSN)' user tmp$sp_definer password '123';
    -- {COMMENT_TAG} ###################################################

    set term ^;
    create or alter procedure sp_chk_mon_access
        returns(
             who_am_i varchar(32)
            ,eff_user varchar(32)
            ,mon_user varchar(32)
            ,executing_sql_blob blob sub_type text
        ) sql security definer
    as
        declare c int;
    begin

        who_am_i =  current_user;

        eff_user = rdb$get_context('SYSTEM', 'EFFECTIVE_USER');

        for
            select
                a.mon$user
               ,s.mon$sql_text
            from rdb$database r
            left join
            ( mon$attachments a
              join mon$statements s on a.mon$attachment_id = s.mon$attachment_id and s.mon$sql_text not like '%RDB$AUTH_MAPPING%'
            ) on 1=1
            where
                a.mon$system_flag is distinct from 1
            into mon_user, executing_sql_blob
        do
        begin
            suspend;
        end

    end
    ^

    create or alter procedure sp_clear_ext_pool
        returns(
             who_am_i varchar(32)
            ,eff_user varchar(32)
            ,access_info varchar(255)
        )  sql security definer
    as
    begin

       who_am_i =  current_user;
       eff_user = rdb$get_context('SYSTEM', 'EFFECTIVE_USER');

       begin
           execute statement 'ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL';
           access_info = 'Ext. pool clear all @@@ PASSED @@@';
       when any do
           begin
               access_info = 'Ext. pool clear all ### FAILED ###, gdscode: ' || gdscode;
           end
       end
       suspend;
    end
    ^

    create or alter procedure sp_chk_sql_rights
        returns(
             who_am_i varchar(32)
            ,eff_user varchar(32)
            ,access_info varchar(255)
        ) sql security definer
    as
        declare c int;
    begin

        who_am_i =  current_user;
        eff_user = rdb$get_context('SYSTEM', 'EFFECTIVE_USER');

        begin
            -- ::: NB ::: exception raised in static PSQL can not be suppressed!
            -- insert into log_table(id, eff_user, mon_user)
            -- values(current_connection, :eff_user, :mon_user);
            execute statement
                ( q'!insert /* current_user: '!'|| current_user || q'!, effective_user: '!'|| eff_user ||q'!' */ into log_table(id, eff_user, mon_user) values( ?, ?, ? )!' )
                ( current_connection, :eff_user, :who_am_i )
            ;
            access_info = 'Insert into log_table @@@ PASSED @@@';
        when any do
            begin
                access_info = 'Insert into log_table ### FAILED ###, gdscode: ' || gdscode;
            end
        end

        suspend;

    end
    ^
    set term ;^
    commit;


    -- {COMMENT_TAG} ###################################################
    connect  '$(DSN)' user sysdba password 'masterkey';
    -- {COMMENT_TAG} ###################################################

    set term ^;
    create or alter procedure sp_chk_syspriv_main (
            a_run_as_who varchar(32)
        )
        returns(
            who_am_i varchar(32)
            ,eff_user varchar(32)
            ,mon_user varchar(32)
            ,executing_sql_blob blob sub_type text
        ) as
    begin
        for
            execute statement
            (
                    'select /* Inside sp_chk_syspriv_main. Enter SP as: ' || current_user
                 || ', run ES as: ' || upper(:a_run_as_who)
                 || ' */ who_am_i, eff_user, mon_user, executing_sql_blob'
                 || ' from sp_chk_mon_access' -- its definer is 'tmp$sp_definer'
            )
            as user :a_run_as_who password '123'
        into
            who_am_i, eff_user, mon_user, executing_sql_blob
        do begin
            suspend;
        end

    end
    ^
    set term ;^
    commit;

    revoke create procedure from tmp$sp_definer;

    grant execute on procedure sp_chk_syspriv_main to public;

    grant execute on procedure sp_chk_mon_access to tmp$outside_caller;
    grant execute on procedure sp_chk_sql_rights to tmp$outside_caller;

    create role r_almost_dba set system privileges to MONITOR_ANY_ATTACHMENT, MODIFY_EXT_CONN_POOL, ACCESS_ANY_OBJECT_IN_DATABASE;
    grant default r_almost_dba to user tmp$almost_dba;

    grant execute on procedure sp_chk_mon_access to tmp$almost_dba;
    grant execute on procedure sp_clear_ext_pool to tmp$almost_dba;

    grant execute on procedure sp_chk_sql_rights to tmp$almost_dba;
    grant select, insert on table log_table to tmp$almost_dba;

    commit;

    set bail off;


    /************************
        1. Usage of MON$ tables

        ... when effective user is *not* privileged and thus it should see only its own attachments,
        in fact it sees only attachments established by the session user.
        In other words, user 'tmp$outside_caller' calls procedure with definer 'tmp$sp_definer'
        which (ooops) sees attachments by user 'tmp$outside_caller' .
    ************************/


    -- {COMMENT_TAG} ###################################################
    connect  '$(DSN)' user tmp$almost_dba password '123';
    -- {COMMENT_TAG} ###################################################

    commit;
    set transaction read committed record_version;

    set term ^;
    execute block as
        declare c varchar(32);
    begin
        execute statement q'!select 'tmp$sp_definer' from rdb$database!'
        as user 'tmp$sp_definer' password '123'
        into c
        ;
    end
    ^
    execute block as
        declare c varchar(32);
    begin
        execute statement q'!select 'tmp$outside_caller' from rdb$database!'
        as user 'tmp$outside_caller' password '123'
        into c
        ;
    end
    ^
    set term ;^

    select -- Invoke sp_chk_syspriv_main. Work as tmp$almost_dba
        'CHECK-1' as msg
        ,p.*
    from rdb$database
    left join sp_chk_syspriv_main('tmp$outside_caller') p on 1=1
    ;
    commit;

    -- {COMMENT_TAG} ###################################################
    connect  '$(DSN)' user sysdba password 'masterkey';
    -- {COMMENT_TAG} ###################################################
    ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL;
    commit;

    -- {COMMENT_TAG} ###################################################
    connect  '$(DSN)' user tmp$sp_definer password '123';
    -- {COMMENT_TAG} ###################################################

    select /* Invoke sp_chk_syspriv_main. Work as tmp$sp_definer */
        'CHECK-2' as msg
        ,p.*
    from  rdb$database
    left join sp_chk_syspriv_main('tmp$outside_caller') p on 1=1
    ;
    commit;


    /************************
        2. System privileges

        ... if *privileged* user 'tmp$almost_dba' calls procedure with non-privileged definer 'tmp$sp_definer',
        it still can perform every DBA action inside that procedure, like if it was executed under 'tmp$almost_dba's
        permissions

    ************************/

    -- {COMMENT_TAG} ###################################################
    connect  '$(DSN)' user tmp$almost_dba password '123';
    -- {COMMENT_TAG} ###################################################

    commit;
    set transaction read committed record_version;

    select /* Before sp_chk_syspriv_main. Work as tmp$almost_dba */
         'CHECK-3' as msg
        ,p.*
    from rdb$database
    left join sp_chk_syspriv_main('tmp$outside_caller') p on 1=1
    ;

    -- Verify that system privilege MODIFY_EXT_CONN_POOL not avaliable
    -- for current user (despite that he is 'tmp$almost_dba') because
    -- SP sp_clear_ext_pool was created with SQL SECURITY DEFINER and
    -- its definer ('tmp$sp_definer') has not any system privilege:
    --
    select
        'CHECK-4' as msg
        ,p.*
    from rdb$database
    left join sp_clear_ext_pool p on 1=1;
    commit;

    -- {COMMENT_TAG} ###################################################
    connect  '$(DSN)' user tmp$almost_dba password '123';
    -- {COMMENT_TAG} ###################################################

    -- Verify that system privilege ACCESS_ANY_OBJECT_IN_DATABASE not avaliable
    -- for current user (despite that he is 'tmp$almost_dba') because
    -- SP sp_chk_sql_rights was created with SQL SECURITY DEFINER and
    -- its definer ('tmp$sp_definer') has not any system privilege:
    --
    select
        'CHECK-5' as msg
        ,p.who_am_i
        ,p.eff_user
        ,p.access_info
    from rdb$database
    left join sp_chk_sql_rights p on 1=1
    ;
    commit;


    -- cleanup:

    -- {COMMENT_TAG} ###################################################
    connect  '$(DSN)' user sysdba password 'masterkey';
    -- {COMMENT_TAG} ###################################################
    ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL;
    drop user tmp$almost_dba;
    drop user tmp$sp_definer;
    drop user tmp$outside_caller;
    commit;

"""

act = isql_act('db', test_script, substitutions=[ ('EXECUTING_SQL_BLOB .* \\d.*', 'EXECUTING_SQL_BLOB'), (f'-- {COMMENT_TAG}.*', '') ])

expected_stdout = """
    MSG                             CHECK-1
    WHO_AM_I                        TMP$OUTSIDE_CALLER
    EFF_USER                        TMP$SP_DEFINER
    MON_USER                        TMP$SP_DEFINER
    EXECUTING_SQL_BLOB
    select 'tmp$sp_definer' from rdb$database

    MSG                             CHECK-2
    WHO_AM_I                        TMP$OUTSIDE_CALLER
    EFF_USER                        TMP$SP_DEFINER
    MON_USER                        TMP$SP_DEFINER
    EXECUTING_SQL_BLOB
    select /* Invoke sp_chk_syspriv_main. Work as tmp$sp_definer */
    'CHECK-2' as msg
    ,p.*
    from  rdb$database
    left join sp_chk_syspriv_main('tmp$outside_caller') p on 1=1

    MSG                             CHECK-3
    WHO_AM_I                        TMP$OUTSIDE_CALLER
    EFF_USER                        TMP$SP_DEFINER
    MON_USER                        <null>
    EXECUTING_SQL_BLOB              <null>

    MSG                             CHECK-4
    WHO_AM_I                        TMP$ALMOST_DBA
    EFF_USER                        TMP$SP_DEFINER
    ACCESS_INFO                     Ext. pool clear all ### FAILED ###, gdscode: 335544351

    MSG                             CHECK-5
    WHO_AM_I                        TMP$ALMOST_DBA
    EFF_USER                        TMP$SP_DEFINER
    ACCESS_INFO                     Insert into log_table ### FAILED ###, gdscode: 335544352
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
