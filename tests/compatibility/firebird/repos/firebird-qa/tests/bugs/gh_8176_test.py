#coding:utf-8

"""
ID:          issue-8176
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8176
TITLE:       Firebird hangs after starting remote profiling session
DESCRIPTION:
    We create two users: tmp_worker_usr and tmp_profiler_usr.
    First of them will perform some 'useful work', second will PROFILE attachment of tmp_worker_usr.
    Also, we create role 'tmp_profiler_role' and grant it to 'tmp_profiler_usr'.
    Ability to profile ather attachment (by tmp_profiler_usr) is achieved via granting to role 
    tmp_profiler_role system privilege PROFILE_ANY_ATTACHMENT.
    Then we create three connections to test DB belonging to tmp_worker_usr, tmp_profiler_usr and SYSDBA.
    We pass attachment_id of tmp_worker_usr to the call 'select rdb$profiler.start_session(...)' performed
    by tmp_profiler_usr. Executing of this causes creation of PLG$PROFILE* tables and views in the test DB
    (they are created by special internal connection - this can be seen in the trace).
    Unfortunately, usedr who calls rdb$profiler.start_session() will *NOT* have any access rights to created
    PLG* objects. This must be donw explicitly to SYSDBA (and this is the reason why we create 3rd connection).
    Finally, we do somewhat similar to example described doc/sql.extensions/README.profiler.md: run two dummy
    procedures by tmp_worker_usr, and after that - call proc rdb$profiler.finish_session by tmp_profiler_usr.
    At the last step, we query data from plg$prof_sessions (it must have number of records equal to the number
    of profiling sessions that we have created).
NOTES:
    [10.07.2024] pzotov
    ::: WARNING :::
    1. On CLASSIC problem still exists: firebird hangs, although new connections to test DB are allowed (found both on 5.x and 6.x).
       One may even to run 'delete from mon$attachments' (using new ISQL session) but there is no effect: server does not perform that.
       Because of this, test currently can be run only on Super.
    2. It looks weird that user (NON-sysdba) who has necessary rights to start profiling, must be explicitly granted to access PLG* tables/views.

    Confirmed bug on 6.0.0.389-cc71c6f (SS), 5.0.1.1432-9d5a60a (SS) - server hanged and did not allow to make new connections to test DB.
    Checked on 6.0.0.392-3fa8e6b, 5.0.1.1434-2593b3b.

    [24.07.2024] pzotov
    Checked fixed problem with hang on Windows, Servermode = CLASSIC
    Commits:
        https://github.com/FirebirdSQL/firebird/commit/fa90256cf080965f92eae11eba8d897c2d02e1b9
            Merge pull request #8186 from FirebirdSQL/work/ProfilerIPC
            Fixed a few issues with IPC used by remote profiler
        https://github.com/FirebirdSQL/firebird/commit/f59905fc29f0b9288d61fc6113fd24301dce1327
            Frontported PR #8186 : Fixed a few issues with IPC used by remote profiler
    Snapshots: 6.0.0.398-f59905f, 5.0.1.1440-7b1b824

    [03.07.2025] pzotov
    Adjusted for FB 6.x: it is MANDATORY to specify schema `PLG$PROFILER.` when querying created profiler tables.
    See doc/sql.extensions/README.schemas.md, section title: '### gbak'; see 'SQL_SCHEMA_PREFIX' variable here.
    Also, on FB 6.x one need to use: 'grant usage on schema "PLG$PROFILER" to role ...'
    Checked on 6.0.0.970; 5.0.3.1668.

    [27.07.2025] pzotov
    Temporary set marker to SKIP on Linux when Servermode = 'Classic' because of hanged execution (since 6.0.0.1066).
    Sent report to dimitr et al, 27.07.2025 10:15.
"""
import os
from firebird.driver import tpb, Isolation # , TraLockResolution, TraAccessMode, DatabaseError
import time

import pytest
from firebird.qa import *

N_COUNT = 1000
init_script = f"""
    create or alter procedure sp_ins as begin end;
    create or alter procedure sp_del as begin end;

    recreate table test (
        id integer primary key,
        val integer not null
    );

    set term ^;

    create or alter function fn_mult(p1 integer, p2 integer) returns integer
    as
    begin
        return p1 * p2;
    end^

    create or alter procedure sp_ins sql security definer
    as
        declare i integer = 1;
    begin
        while (i <= {N_COUNT})
        do
        begin
            if (mod(i, 2) = 1) then
                insert into test values (:i, fn_mult(:i, 2));
            i = i + 1;
        end
    end
    ^

    create or alter procedure sp_del sql security definer
    as
        declare i integer = 1;
    begin
        while (i <= {N_COUNT} / 2)
        do
        begin
            if (mod(i, 2) = 1) then
                delete from test where id = fn_mult(:i, 2);
            i = i + 1;
        end
    end
    ^
    set term ;^
    commit;

    grant execute on procedure sp_ins to public;
    grant execute on procedure sp_del to public;
    commit;
"""

tmp_worker_usr = user_factory('db', name='tmp_worker_8176', password='123')
tmp_profiler_usr = user_factory('db', name='tmp_profiler_8176', password='456')
tmp_profiler_role = role_factory('db', name='tmp_profiler_role')

db = db_factory(init=init_script)
act = python_act('db')

@pytest.mark.disabled_in_forks
@pytest.mark.version('>=5.0.1')
def test_1(act: Action, tmp_worker_usr: User, tmp_profiler_usr: User, tmp_profiler_role: Role, capsys):

    if os.name != 'nt' and act.vars['server-arch'] != 'SuperServer':
        pytest.skip("TEMPORARY SKIPPED BECAUSE HANGS")
    
    PLG_SCHEMA_PREFIX = '' if act.is_version('<6') else f'PLG$PROFILER.'

    addi_script = f"""
        set wng off;
        set bail on;
        alter user {tmp_profiler_usr.name} revoke admin role;
        revoke all on all from {tmp_profiler_usr.name};
        commit;
        -- doc/sql.extensions/README.profiler.md:
        -- If the remote attachment is from a different user, the calling user must have the system privilege `PROFILE_ANY_ATTACHMENT`.
        alter role {tmp_profiler_role.name}
            set system privileges to PROFILE_ANY_ATTACHMENT;
        commit;
        grant default {tmp_profiler_role.name} to user {tmp_profiler_usr.name};
        commit;
    """
    act.isql(switches=['-q'], input=addi_script)

    custom_tpb = tpb(isolation = Isolation.READ_COMMITTED_READ_CONSISTENCY, lock_timeout = 0)

    with act.db.connect() as con_admin, \
         act.db.connect(user = tmp_worker_usr.name, password = tmp_worker_usr.password) as con_worker, \
         act.db.connect(user = tmp_profiler_usr.name, password = tmp_profiler_usr.password, role = tmp_profiler_role.name) as con_profiler:

        cur_worker = con_worker.cursor()
        cur_worker.execute('select current_connection from rdb$database')
        worker_attach_id = cur_worker.fetchone()[0]

        tx_profiler = con_profiler.transaction_manager(custom_tpb)
        cur_profiler = tx_profiler.cursor()

        #..............................................................................
        
        tx_profiler.begin()
        cur_profiler.execute(f"select rdb$profiler.start_session('remote_profiling_of_inserts', null, {worker_attach_id}) from rdb$database;")
        cur_profiler.fetchall()

        cur_worker.callproc('sp_ins')

        cur_profiler.callproc(f'rdb$profiler.finish_session(true, {worker_attach_id})')
        tx_profiler.commit()

        #..............................................................................

        tx_profiler.begin()
        cur_profiler.execute(f"select rdb$profiler.start_session('remote_profiling_of_deletions', null, {worker_attach_id}) from rdb$database;")
        cur_profiler.fetchall()

        cur_worker.callproc('sp_del')

        cur_profiler.callproc(f'rdb$profiler.finish_session(true, {worker_attach_id})')
        tx_profiler.commit()

        #------------------------------------------------------------------------------
        # firebird.driver.types.DatabaseError: no permission for SELECT access to TABLE PLG$PROF_SESSIONS
        # -Effective user is TMP_USER_PROFILER_ANY_ATT
        #################################
        # ::: NB ::: Why this is needed ?
        #################################

        # firebird.driver.types.DatabaseError: no permission for USAGE access to SCHEMA "PLG$PROFILER"
        # -Effective user is TMP_PROFILER_8176
        if act.is_version('<6'):
            pass
        else:
            con_admin.execute_immediate(f'grant usage on schema "{PLG_SCHEMA_PREFIX[:-1]}" to role {tmp_profiler_role.name}')

        con_admin.execute_immediate(f'grant select on {PLG_SCHEMA_PREFIX}plg$prof_sessions to role {tmp_profiler_role.name}')
        con_admin.execute_immediate(f'grant select on {PLG_SCHEMA_PREFIX}plg$prof_psql_stats_view to role {tmp_profiler_role.name}')
        con_admin.commit()
        #------------------------------------------------------------------------------

        tx_profiler.begin()
        cur_profiler.execute(f'select description as profiler_session_descr, attachment_id as profiled_attachment, trim(user_name) as who_was_profiled from {PLG_SCHEMA_PREFIX}plg$prof_sessions order by profile_id')
        cur_cols = cur_profiler.description
        for r in cur_profiler:
            for i in range(0,len(cur_cols)):
                print( cur_cols[i][0], ':', r[i] )
        tx_profiler.commit()

    act.expected_stdout = f"""
        PROFILER_SESSION_DESCR : remote_profiling_of_inserts
        PROFILED_ATTACHMENT : {worker_attach_id}
        WHO_WAS_PROFILED : {tmp_worker_usr.name.upper()}

        PROFILER_SESSION_DESCR : remote_profiling_of_deletions
        PROFILED_ATTACHMENT : {worker_attach_id}
        WHO_WAS_PROFILED : {tmp_worker_usr.name.upper()}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
