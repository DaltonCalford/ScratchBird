#coding:utf-8

"""
ID:          issue-3478
ISSUE:       3478
TITLE:       Wait mode and lock timeout of external transaction of EXECUTE STATEMENT not
  matched to corresponding parameters of local transaction
DESCRIPTION:
JIRA:        CORE-3100
FBTEST:      bugs.core_3100
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter procedure sp_add_trn_data (a_run_no smallint) as begin end;
    commit;

    recreate table mon_trn(
        run_no         smallint,
        att_id         bigint,
        trn_id         bigint,
        state          bigint,
        dts            timestamp,
        top_trn        bigint,
        oldest_trn     bigint,
        oldest_active  bigint,
        isolation_mode bigint,
        lock_timeout   bigint,
        read_only      bigint,
        auto_commit    bigint,
        auto_undo      bigint,
        stat_id        bigint,
        usr            char(31) character set unicode_fss
    );
    commit;

    set term ^;
    create or alter procedure sp_add_trn_data (a_run_no smallint) as
        declare v_dbname type of column mon$database.mon$database_name;
        declare v_stt varchar(1024);
        declare v_usr1 type of column mon_trn.usr = 'tmp$c3100a';
        declare v_pwd1 varchar(20) = 'tmp$c3100a';
        declare v_usr2 type of column mon_trn.usr = 'tmp$c3100b';
        declare v_pwd2 varchar(20) = 'tmp$c3100b';
    begin
        v_stt =
               'insert into mon_trn( '
            || '     run_no'
            || '    ,att_id'
            || '    ,trn_id'
            || '    ,state'
            || '    ,dts'
            || '    ,top_trn'
            || '    ,oldest_trn'
            || '    ,oldest_active'
            || '    ,isolation_mode'
            || '    ,lock_timeout'
            || '    ,read_only'
            || '    ,auto_commit'
            || '    ,auto_undo'
            || '    ,stat_id'
            || '    ,usr'
            || ')'
            || ' select'
            || '     :x'
            || '    ,t.mon$attachment_id'
            || '    ,t.mon$transaction_id'
            || '    ,t.mon$state'
            || '    ,t.mon$timestamp'
            || '    ,t.mon$top_transaction'
            || '    ,t.mon$oldest_transaction'
            || '    ,t.mon$oldest_active'
            || '    ,t.mon$isolation_mode'
            || '    ,t.mon$lock_timeout'
            || '    ,t.mon$read_only'
            || '    ,t.mon$auto_commit'
            || '    ,t.mon$auto_undo'
            || '    ,t.mon$stat_id'
            || '    ,a.mon$user'
            || ' from mon$transactions t join mon$attachments a using (mon$attachment_id)'
            || ' where mon$transaction_id = current_transaction'
            ;

        --- 1 --- direct statement, sysdba
        insert into mon_trn(
             run_no
            ,att_id
            ,trn_id
            ,state
            ,dts
            ,top_trn
            ,oldest_trn
            ,oldest_active
            ,isolation_mode
            ,lock_timeout
            ,read_only
            ,auto_commit
            ,auto_undo
            ,stat_id
            ,usr
        )
        select
             :a_run_no
            ,t.mon$attachment_id
            ,t.mon$transaction_id
            ,t.mon$state
            ,t.mon$timestamp
            ,t.mon$top_transaction
            ,t.mon$oldest_transaction
            ,t.mon$oldest_active
            ,t.mon$isolation_mode
            ,t.mon$lock_timeout
            ,t.mon$read_only
            ,t.mon$auto_commit
            ,t.mon$auto_undo
            ,t.mon$stat_id
            ,a.mon$user
        from mon$transactions t join mon$attachments a using (mon$attachment_id)
        where mon$transaction_id = current_transaction;

        select 'localhost:' || d.mon$database_name
        from mon$database d
        into v_dbname;

        --- 2 --- execute statement on EXTERNAL datasource, with new attachment because of user = tmp$c3100a
        execute statement ( v_stt ) ( x:= :a_run_no )
        on external :v_dbname
        as user :v_usr1 password :v_pwd1
        ;

        --- 3 --- execute statement on EXTERNAL datasource, AUTONOMOUS transaction, with new attachment because user = tmp$c3100b
        execute statement ( v_stt ) ( x:= :a_run_no )
        on external :v_dbname
        as user :v_usr2 password :v_pwd2
        with autonomous transaction
        ;
    end
    ^
    set term ;^
    commit;

"""

db = db_factory(init=init_script)

test_script = """
    drop user tmp$c3100a;
    commit;
    create user tmp$c3100a password 'tmp$c3100a';
    commit;
    grant select,insert on mon_trn to tmp$c3100a;
    commit;

    drop user tmp$c3100b;
    commit;
    create user tmp$c3100b password 'tmp$c3100b';
    commit;
    grant select,insert on mon_trn to tmp$c3100b;
    commit;

    -- Check for 'WAIT':
    set transaction wait;
    execute procedure sp_add_trn_data(1);
    commit;

    -- Check for 'NO WAIT':
    set transaction no wait;
    execute procedure sp_add_trn_data(2);
    commit;

    -- Check for 'LOCK TIMEOUT' value:
    set transaction lock timeout 9;
    execute procedure sp_add_trn_data(3);
    commit;


    -- Check for ISOLATION level:
    set transaction read committed no wait;
    execute procedure sp_add_trn_data(4);
    commit;

    -- DO NOT: as of 26.03.2015, flag NO_AUTO_UNDO should *NOT* be copied
    -- (commented after colsulting with Vlad)
    -- Check for NO AUTO UNDO:
    -- set transaction read committed no wait no auto undo;
    -- execute procedure sp_add_trn_data(5);
    -- commit;

    set list on;
    select
        m.run_no
        ,count(distinct m.trn_id) trn_distinct_count
        ,count(distinct m.lock_timeout) wait_distinct_count
        ,count(distinct m.isolation_mode) isol_distinct_count
    from mon_trn m
    group by 1
    order by 1;
    commit;

    drop user tmp$c3100a;
    drop user tmp$c3100b;
    commit;

    --                                    ||||||||||||||||||||||||||||
    -- ###################################|||  FB 4.0+, SS and SC  |||##############################
    --                                    ||||||||||||||||||||||||||||
    -- If we check SS or SC and ExtConnPoolLifeTime > 0 (config parameter FB 4.0+) then current
    -- DB (bugs.core_NNNN.fdb) will be 'captured' by firebird.exe process and fbt_run utility
    -- will not able to drop this database at the final point of test.
    -- Moreover, DB file will be hold until all activity in firebird.exe completed and AFTER this
    -- we have to wait for <ExtConnPoolLifeTime> seconds after it (discussion and small test see
    -- in the letter to hvlad and dimitr 13.10.2019 11:10).
    -- This means that one need to kill all connections to prevent from exception on cleanup phase:
    -- SQLCODE: -901 / lock time-out on wait transaction / object <this_test_DB> is in use
    -- #############################################################################################
    delete from mon$attachments where mon$attachment_id != current_connection;
    commit;
"""

act = isql_act('db', test_script,
                 substitutions=[('Statement failed, SQLSTATE.*', 'Statement failed'),
                                ('record not found for user:.*', 'record not found for user')])

expected_stdout = """
    RUN_NO                          1
    TRN_DISTINCT_COUNT              3
    WAIT_DISTINCT_COUNT             1
    ISOL_DISTINCT_COUNT             1

    RUN_NO                          2
    TRN_DISTINCT_COUNT              3
    WAIT_DISTINCT_COUNT             1
    ISOL_DISTINCT_COUNT             1

    RUN_NO                          3
    TRN_DISTINCT_COUNT              3
    WAIT_DISTINCT_COUNT             1
    ISOL_DISTINCT_COUNT             1

    RUN_NO                          4
    TRN_DISTINCT_COUNT              3
    WAIT_DISTINCT_COUNT             1
    ISOL_DISTINCT_COUNT             1
"""

expected_stderr = """
Statement failed, SQLSTATE = HY000
record not found for user: TMP$C3100A
Statement failed, SQLSTATE = HY000
record not found for user: TMP$C3100B
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)
