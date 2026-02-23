#coding:utf-8

"""
ID:          session.ext-conn-pool-01
TITLE:       External Connections Pool, functionality test 01
DESCRIPTION:
    Basic check of External Connections Pool. We verify here following:
    * ability to reuse connections from ECP in case running ES/EDS by "frequent" attachments
    * ability to distinguish connect/disconnect from reuse connections within apropriate
      DB-level trigger (system variable RESETTING = faluse | true)
    * ability to get information about ECP state: total number of active and idle connections.

    Test creates several DB objects:
        * DB-level triggers on CONNECT / DISCONNECT;
        * table 'ecp_audit' for logging these events, with specifying detailed info: whether current
          connect/disconnect is caused by SESSION RESET (variable RESETTING is TRUE) or no;
        * two users who which further perform connect and run several ES/EDS statements:
            'tmp_ecp_freq' -- will do ES/EDS 'frequently', i.e. with interval LESS than ExtConnPoolLifeTime;
            'tmp_ecp_rare' -- will do ES/EDS 'rarely', with interval GREATER than ExtConnPoolLifeTime;
        * role 'cleaner_ext_pool' with granting to it system privilege MODIFY_EXT_CONN_POOL, in order
          to have ability to clear ext pool after final ES/EDS. Grant this role to both users.
    
    Then we create several connections for user 'freq' (appending them into a list) and for each of them
    do ES/EDS. Number of connections is specified by variable ITER_LOOP_CNT. Delay between subsequent
    ES/EDS for 'freq' is minimal: 1 second.
    After this, we repeate the same for user 'rare', and use delay between subsequent ES/EDS GREATER
    than ExtConnPoolLifeTime for <ADD_DELAY_FOR_RARE> seconds.
    After loop we clear ExtConnPool and close all connections from list.

FBTEST:      functional.session.ext_conn_pool_01
NOTES:

    Discussion in fb-devel related to ECP: 18.05.2018 ... 29.05.2018
    See also: https://github.com/FirebirdSQL/firebird/issues/6093
    ("Implement way to reset user session environment to its initial (default) state", ex. CORE-5832)

    DB-level triggers fire on RESET session event:
        https://github.com/FirebirdSQL/firebird/commit/752424d51384bfe137d3e8e0aff293ff3c5e5ae2
        30-NOV-2020
        Addition for CORE-5832 : Implement way to reset user session environment to its initial (default) state
        Database triggers ON DISCONNECT and ON CONNECT is fired during session reset.
        New system variable RESETTING is introduced to detect session reset state.

    Explanation by Vlad for Classic (why ECP counters active = idle = 0) -- letter 05-jan-2021 14:48, subj:
        "Classic 4.x: can't understand why RESETTING = true in [DIS]CONNECT trigger"

    Checked on 5.0.0.623, 4.0.1.2692 - both on Windows and Linux.
"""
import locale
import re
import time
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

tmp_user_freq = user_factory('db', name='tmp_ecp_freq', password='123', plugin='Srp')
tmp_user_rare = user_factory('db', name='tmp_ecp_rare', password='123', plugin='Srp')
tmp_cleaner_role = role_factory('db', name='cleaner_ext_pool')

# How many seconds will be added to delay = <ECP_LIFE> when user 'RARE' works with database.
#
ADD_DELAY_FOR_RARE = 2

# How many connections will be done by users 'FREQ' and (after him) by 'RARE'.
# Each connection will run _single_ DML using ES/EDS and then immediately is closed
# Subsequent connection will run its DML after N seconds where:
# N = 1  -- for user 'FREQ'
# N = ECP_LIFE + ADD_DELAY_FOR_RARE  -- for user 'RARE'
#
ITER_LOOP_CNT = 3

@pytest.mark.es_eds
@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user_freq: User, tmp_user_rare: User, tmp_cleaner_role: Role, capsys):

    # [doc] state of external connections pool could be queried using ...:
    # - EXT_CONN_POOL_SIZE			pool size
    # - EXT_CONN_POOL_LIFETIME		idle connection lifetime, in seconds
    ECP_SIZE, ECP_LIFE = -1, -1
    with act.db.connect() as con:
        with con.cursor() as cur:
            cur.execute("select cast(rdb$get_context('SYSTEM', 'EXT_CONN_POOL_SIZE') as int), cast(rdb$get_context('SYSTEM', 'EXT_CONN_POOL_LIFETIME') as int) from rdb$database")
            ECP_SIZE, ECP_LIFE = cur.fetchone()
    assert ECP_SIZE > 1;

    SERVER_MODE = act.get_server_architecture()
    sql_init = f'''
        set bail on;
        set list on;
        set wng off;
        alter database set linger to {ECP_LIFE+ADD_DELAY_FOR_RARE+2};
        alter role {tmp_cleaner_role.name} set system privileges to MODIFY_EXT_CONN_POOL;
        commit;

        grant default {tmp_cleaner_role.name} to user {tmp_user_freq.name};
        grant default {tmp_cleaner_role.name} to user {tmp_user_rare.name};
        commit;

        set term ^;
        create function fn_get_bin_name() returns varchar(255) as
            declare v_bin_name varchar(255);
        begin
            v_bin_name = rdb$get_context('SYSTEM','CLIENT_PROCESS');

            -- ::: NOTE ::: Python binary can have numeric suffixes on Linux:
            if (v_bin_name similar to '%((\\python([[:DIGIT:]]+%)?.exe)|(/python([[:DIGIT:]]+%)?))') then
                return 'python'; -- '/usr/bin/python3.9' ==> 'python'

            if (v_bin_name similar to '%((\\firebird([[:DIGIT:]]+%)?.exe)|(/python([[:DIGIT:]]+%)?))') then
                return 'firebird';

            -- Extracts file name from full path of client binary process.
            -- For Windows: removes extension, in order returning name be the same as on Linux.
            -- 'c:\\program files\\firebird30\\firebird.exe' --> 'firebird'
            -- '/opt/firebird/bin/firebird'                  --> 'firebird'
            -- ::: NOTE ::: backslash must be duplicated when using this DDL in any Python
            -- environment otherwise it will be swallowed
            return
            (
                select
                    --p
                    --,r
                    --,ext_pos
                    --,n
                    --,x
                    reverse(left(x,n)) as clnt_bin
                    -- reverse(x)  as clnt_bin
                from
                (
                    select
                        p
                        ,reverse(p) r
                        ,ext_pos
                        ,substring(reverse(p) from iif( is_win, ext_pos+1, 1) ) x
                        ,position( '|' in replace(replace(reverse( p ),'\\','|'),'/','|') ) - iif(is_win, ext_pos+1, 1) as n
                    from (
                        select
                            trim(p) as p
                            ,position( '\\' in p ) > 0 as is_win
                            ,iif( position( '\\' in p ) > 0, position('.' in reverse(trim(p))), 0) as ext_pos
                        from (
                            select rdb$get_context('SYSTEM','CLIENT_PROCESS') as p from rdb$database
                        )
                    )
                )
            );

        end
        ^
        set term ;^


        create table ecp_audit(
             id smallint generated by default as identity constraint pk_audit primary key
            ,srvmode varchar(12) -- 'Super' / 'SuperClassic' / 'Classic'
            ,who varchar(12) default current_user -- 'TMP_ECP_FREQ' / 'TMP_ECP_RARE' / 'SYSDBA'
            ,evt varchar(40) not null
            ,att smallint default current_connection
            ,trn smallint default current_transaction
            ,dts timestamp default 'now'
            ,pool_active_count smallint
            ,pool_idle_count smallint
            ,clnt_bin varchar(8)
            ,aux_info varchar(100)
        );

        create view v_ecp_audit as
        select
            who
           ,att
           ,id
           ,evt
           ,active_cnt
           ,idle_cnt
           ,clnt_bin
        from (
            select
                 srvmode
                ,who
                ,cast(dense_rank()over(partition by who order by att) as smallint) as att
                ,cast(dense_rank()over(partition by who order by id) as smallint) as id
                ,evt
                ,trn
                ,pool_active_count as active_cnt
                ,pool_idle_count as idle_cnt
                ,clnt_bin
            from ecp_audit
        )
        order by who, id
        ;


        grant select,insert on ecp_audit to public;
        grant select on v_ecp_audit to public;
        commit;

        set term ^;
        create or alter trigger trg_aud_bi for ecp_audit active before insert sql security definer as
            declare v_srvmode varchar(30);
            declare p int;
        begin
            new.srvmode = '{SERVER_MODE}';
            new.pool_active_count = rdb$get_context('SYSTEM','EXT_CONN_POOL_ACTIVE_COUNT');
            new.pool_idle_count = rdb$get_context('SYSTEM','EXT_CONN_POOL_IDLE_COUNT');
            new.clnt_bin = right(fn_get_bin_name(), 8);
            -- right(rdb$get_context('SYSTEM','CLIENT_PROCESS'),15);
        end
        ^

        create or alter trigger trg_connect inactive on connect sql security definer as
            declare p smallint;
        begin
            if (current_user <> '{act.db.user}') then
            begin

                insert into ecp_audit(
                    evt
                ) values (
                    iif(resetting, 'TAKE FROM POOL: IDLE -> ACTIVE', 'NEW')
                );
            end
        end
        ^

        create or alter trigger trg_disconnect inactive on disconnect sql security definer as
        begin
            if (current_user <> '{act.db.user}') then
            begin
                insert into ecp_audit(
                    evt
                ) values (
                    iif(resetting, 'MOVE INTO POOL: ACTIVE -> IDLE', 'BYE')
                );
            end
        end
        ^
        set term ;^
        commit;
        alter trigger trg_connect active;
        alter trigger trg_disconnect active;
        commit;
    '''

    act.expected_stdout = ''
    act.isql(switches = ['-q'], input = sql_init, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #-----------------------------------------------------

    sql_for_run = '''
        execute block as
            declare c int;
        begin
            execute statement ( q'{ insert into ecp_audit( evt ) values( 'RUN DML') }' )
            on external
               'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
            with autonomous transaction -- <<< !!! THIS IS MANDATORY IF WE WANT TO USE EXT CONN POOL !!! <<<
            as user current_user password '123'
            ;
        end
    '''

    for usr in (tmp_user_freq, tmp_user_rare):
        conn_list = []
        for i in range(0, ITER_LOOP_CNT):
            conn_list.append( act.db.connect(user = usr.name, password = usr.password) )

        for i,c in enumerate(conn_list):

            # ::: NOTE :::
            # ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
            # On every iteration DIFFERENT connection is used for run ES/EDS,
            # but all of them use the same user/password/role, so apropriate
            # item in the ExtConnPool can be used to run this statement.
            # But this will be so only for user = 'FREQ' because he does such
            # actions 'frequently': each (<ECP_LIFE> - 2) seconds.
            # For user 'RARE' new attachment will be created every time when
            # he runs ES/EDS because he does that 'rarely' and idle connection
            # (from his previous iteration) is removed from ExtConnPool due to
            # expiration of ExtConnPoolLifeTime:
            # ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
            try:
                #c.execute_immediate( sql_for_run )
                with c.cursor() as cur:
                    cur.execute(sql_for_run)

                if i < len(conn_list)-1:
                    time.sleep( 1 if usr == tmp_user_freq else ECP_LIFE + ADD_DELAY_FOR_RARE )
                else:
                    c.execute_immediate( 'ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL' )
            finally:
                if c:
                    c.close()

    if SERVER_MODE == 'Classic':
        act.expected_stdout = """
            WHO              ATT      ID EVT                                      ACTIVE_CNT IDLE_CNT CLNT_BIN
            ============ ======= ======= ======================================== ========== ======== ========
            TMP_ECP_FREQ       1       1 NEW                                               0        0 python
            TMP_ECP_FREQ       2       2 NEW                                               0        0 python
            TMP_ECP_FREQ       3       3 NEW                                               0        0 python
            TMP_ECP_FREQ       4       4 NEW                                               0        0 firebird
            TMP_ECP_FREQ       4       5 RUN DML                                           0        0 firebird
            TMP_ECP_FREQ       4       6 MOVE INTO POOL: ACTIVE -> IDLE                    0        0 firebird
            TMP_ECP_FREQ       4       7 TAKE FROM POOL: IDLE -> ACTIVE                    0        0 firebird
            TMP_ECP_FREQ       1       8 BYE                                               0        1 python
            TMP_ECP_FREQ       5       9 NEW                                               0        0 firebird
            TMP_ECP_FREQ       5      10 RUN DML                                           0        0 firebird
            TMP_ECP_FREQ       5      11 MOVE INTO POOL: ACTIVE -> IDLE                    0        0 firebird
            TMP_ECP_FREQ       5      12 TAKE FROM POOL: IDLE -> ACTIVE                    0        0 firebird
            TMP_ECP_FREQ       4      13 BYE                                               0        0 firebird
            TMP_ECP_FREQ       2      14 BYE                                               0        1 python
            TMP_ECP_FREQ       6      15 NEW                                               0        0 firebird
            TMP_ECP_FREQ       6      16 RUN DML                                           0        0 firebird
            TMP_ECP_FREQ       6      17 MOVE INTO POOL: ACTIVE -> IDLE                    0        0 firebird
            TMP_ECP_FREQ       6      18 TAKE FROM POOL: IDLE -> ACTIVE                    0        0 firebird
            TMP_ECP_FREQ       6      19 BYE                                               0        0 firebird
            TMP_ECP_FREQ       3      20 BYE                                               0        0 python
            TMP_ECP_FREQ       5      21 BYE                                               0        0 firebird
            TMP_ECP_RARE       1       1 NEW                                               0        0 python
            TMP_ECP_RARE       2       2 NEW                                               0        0 python
            TMP_ECP_RARE       3       3 NEW                                               0        0 python
            TMP_ECP_RARE       4       4 NEW                                               0        0 firebird
            TMP_ECP_RARE       4       5 RUN DML                                           0        0 firebird
            TMP_ECP_RARE       4       6 MOVE INTO POOL: ACTIVE -> IDLE                    0        0 firebird
            TMP_ECP_RARE       4       7 TAKE FROM POOL: IDLE -> ACTIVE                    0        0 firebird
            TMP_ECP_RARE       4       8 BYE                                               0        0 firebird
            TMP_ECP_RARE       1       9 BYE                                               0        0 python
            TMP_ECP_RARE       5      10 NEW                                               0        0 firebird
            TMP_ECP_RARE       5      11 RUN DML                                           0        0 firebird
            TMP_ECP_RARE       5      12 MOVE INTO POOL: ACTIVE -> IDLE                    0        0 firebird
            TMP_ECP_RARE       5      13 TAKE FROM POOL: IDLE -> ACTIVE                    0        0 firebird
            TMP_ECP_RARE       5      14 BYE                                               0        0 firebird
            TMP_ECP_RARE       2      15 BYE                                               0        0 python
            TMP_ECP_RARE       6      16 NEW                                               0        0 firebird
            TMP_ECP_RARE       6      17 RUN DML                                           0        0 firebird
            TMP_ECP_RARE       6      18 MOVE INTO POOL: ACTIVE -> IDLE                    0        0 firebird
            TMP_ECP_RARE       6      19 TAKE FROM POOL: IDLE -> ACTIVE                    0        0 firebird
            TMP_ECP_RARE       6      20 BYE                                               0        0 firebird
            TMP_ECP_RARE       3      21 BYE                                               0        0 python
        """
    else:
        act.expected_stdout = """
            WHO              ATT      ID EVT                                      ACTIVE_CNT IDLE_CNT CLNT_BIN
            ============ ======= ======= ======================================== ========== ======== ========
            TMP_ECP_FREQ       1       1 NEW                                               0        0 python
            TMP_ECP_FREQ       2       2 NEW                                               0        0 python
            TMP_ECP_FREQ       3       3 NEW                                               0        0 python
            TMP_ECP_FREQ       4       4 NEW                                               0        0 firebird
            TMP_ECP_FREQ       4       5 RUN DML                                           1        0 firebird
            TMP_ECP_FREQ       4       6 MOVE INTO POOL: ACTIVE -> IDLE                    1        0 firebird
            TMP_ECP_FREQ       4       7 TAKE FROM POOL: IDLE -> ACTIVE                    1        0 firebird
            TMP_ECP_FREQ       1       8 BYE                                               0        1 python
            TMP_ECP_FREQ       4       9 RUN DML                                           1        0 firebird
            TMP_ECP_FREQ       4      10 MOVE INTO POOL: ACTIVE -> IDLE                    1        0 firebird
            TMP_ECP_FREQ       4      11 TAKE FROM POOL: IDLE -> ACTIVE                    1        0 firebird
            TMP_ECP_FREQ       2      12 BYE                                               0        1 python
            TMP_ECP_FREQ       4      13 RUN DML                                           1        0 firebird
            TMP_ECP_FREQ       4      14 MOVE INTO POOL: ACTIVE -> IDLE                    1        0 firebird
            TMP_ECP_FREQ       4      15 TAKE FROM POOL: IDLE -> ACTIVE                    1        0 firebird
            TMP_ECP_FREQ       4      16 BYE                                               0        0 firebird
            TMP_ECP_FREQ       3      17 BYE                                               0        0 python
            TMP_ECP_RARE       1       1 NEW                                               0        0 python
            TMP_ECP_RARE       2       2 NEW                                               0        0 python
            TMP_ECP_RARE       3       3 NEW                                               0        0 python
            TMP_ECP_RARE       4       4 NEW                                               0        0 firebird
            TMP_ECP_RARE       4       5 RUN DML                                           1        0 firebird
            TMP_ECP_RARE       4       6 MOVE INTO POOL: ACTIVE -> IDLE                    1        0 firebird
            TMP_ECP_RARE       4       7 TAKE FROM POOL: IDLE -> ACTIVE                    1        0 firebird
            TMP_ECP_RARE       4       8 BYE                                               0        0 firebird
            TMP_ECP_RARE       1       9 BYE                                               0        0 python
            TMP_ECP_RARE       5      10 NEW                                               0        0 firebird
            TMP_ECP_RARE       5      11 RUN DML                                           1        0 firebird
            TMP_ECP_RARE       5      12 MOVE INTO POOL: ACTIVE -> IDLE                    1        0 firebird
            TMP_ECP_RARE       5      13 TAKE FROM POOL: IDLE -> ACTIVE                    1        0 firebird
            TMP_ECP_RARE       5      14 BYE                                               0        0 firebird
            TMP_ECP_RARE       2      15 BYE                                               0        0 python
            TMP_ECP_RARE       6      16 NEW                                               0        0 firebird
            TMP_ECP_RARE       6      17 RUN DML                                           1        0 firebird
            TMP_ECP_RARE       6      18 MOVE INTO POOL: ACTIVE -> IDLE                    1        0 firebird
            TMP_ECP_RARE       6      19 TAKE FROM POOL: IDLE -> ACTIVE                    1        0 firebird
            TMP_ECP_RARE       6      20 BYE                                               0        0 firebird
            TMP_ECP_RARE       3      21 BYE                                               0        0 python
        """
    act.isql(switches = ['-q', '-pag', '999999'], input = 'select who,att,id,evt,active_cnt,idle_cnt,clnt_bin from v_ecp_audit;', combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

