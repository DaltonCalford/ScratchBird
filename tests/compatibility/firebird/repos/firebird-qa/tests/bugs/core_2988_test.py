#coding:utf-8

"""
ID:          issue-3370
ISSUE:       3370
TITLE:       Concurrent transaction number not reported if lock timeout occurs
DESCRIPTION:
NOTES:
[08-aug-2018] ::: ACHTUNG :::
  Important change has been added in FB 4.0.
  According to doc\\README.read_consistency.md, read committed isolation level
  was modified and new transaction with RC effectively is launched like:
  SET TRANSACTION READ COMMITTED READ CONSISTENCY
  Moreover, it is unable to start transaction in NO_record_version until
  config parameter ReadConsistency will be changed from 1(default) to 0.
  This mean that now it is unable to use NO_record_version setting in RC mode
  with default firebird.conf ==> we can not check behaviour of engine exactly
  as ticket says in its case-1:
  ===
    set transaction read committed no record_version lock timeout 10;
    select * from test;
  ===
  For this reason it was decided to create separate section for major version 4.0
  and use UPDATE statement instead of 'select * from test' (UPDATE also must READ
  data before changing).
JIRA:        CORE-2988
FBTEST:      bugs.core_2988
"""

import pytest
from firebird.qa import *
from firebird.driver import TPB, TraAccessMode, Isolation

# version: 3.0

substitutions_1 = [('record not found for user:.*', ''),
                   ('-concurrent transaction number is.*', '-concurrent transaction number is'),
                   ('-At block line: [\\d]+, col: [\\d]+', '-At block line')]

init_script_1 = """"""

db_1 = db_factory(sql_dialect=3, init=init_script_1)

test_script_1 = """
    drop user tmp$2988;
    commit;
    create user tmp$2988 password 'tmp$2988';
    commit;

    recreate table test (id integer);
    insert into test values(1);
    commit;

    revoke all on all from tmp$2988;
    grant select,update on test to tmp$2988;
    commit;

    set transaction lock timeout 1;
    update test set id = id;

    set term ^;
    execute block as
        declare v_usr char(31) = 'tmp$2988';
        declare v_pwd varchar(20) = 'tmp$2988';
        declare v_connect varchar(255);
        declare v_dummy int;
    begin
        execute statement ('update test set id = -1')
        with autonomous transaction
        as user v_usr password v_pwd
        into v_dummy;
    end
    ^
    set term ;^
    rollback;

    set transaction read committed no record_version lock timeout 1;
    update test set id = id;

    set term ^;
    execute block as
        declare v_usr char(31) = 'tmp$2988';
        declare v_pwd varchar(20) = 'tmp$2988';
        declare v_connect varchar(255);
        declare v_dummy int;
    begin
        execute statement ('select id from test')
        with autonomous transaction
        as user v_usr password v_pwd
        into v_dummy;
    end
    ^
    set term ;^
    rollback;

    set list on;
    set transaction read committed no record_version lock timeout 1;
    select id from test with lock;

    set term ^;
    execute block as
        declare v_usr char(31) = 'tmp$2988';
        declare v_pwd varchar(20) = 'tmp$2988';
        declare v_connect varchar(255);
        declare v_dummy int;
    begin
        begin
            v_dummy = rdb$get_context('SYSTEM', 'EXT_CONN_POOL_SIZE');
            rdb$set_context('USER_SESSION', 'EXT_CONN_POOL_SUPPORT','1');
        when any do
            begin
            end
        end

        execute statement ('select id from test with lock')
        with autonomous transaction
        as user v_usr password v_pwd
        into v_dummy;
    end
    ^
    drop user tmp$2988
    ^
    commit
    ^
    --                                    ||||||||||||||||||||||||||||
    -- ###################################|||  HQBird 3.x  SS/SC   |||##############################
    --                                    ||||||||||||||||||||||||||||
    -- If we check SS or SC and ExtConnPoolLifeTime > 0 (avaliable in HQbird 3.x) then current
    -- DB (bugs.core_NNNN.fdb) will be 'captured' by firebird.exe process and fbt_run utility
    -- will not able to drop this database at the final point of test.
    -- Moreover, DB file will be hold until all activity in firebird.exe completed and AFTER this
    -- we have to wait for <ExtConnPoolLifeTime> seconds after it (discussion and small test see
    -- in the letter to hvlad and dimitr 13.10.2019 11:10).
    -- This means that one need to kill all connections to prevent from exception on cleanup phase:
    -- SQLCODE: -901 / lock time-out on wait transaction / object <this_test_DB> is in use
    -- #############################################################################################
    execute block as
    begin
        if ( rdb$get_context('USER_SESSION', 'EXT_CONN_POOL_SUPPORT') = '1' ) then
        begin
            -- HQbird is tested now:
            -- execute statement 'delete from mon$attachments where mon$attachment_id != current_connection';
            execute statement 'ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL';
        end
    end
    ^
    commit
    ^
"""

act_1 = isql_act('db_1', test_script_1, substitutions=substitutions_1)

expected_stdout_1 = """
    ID                              1
"""

expected_stderr_1 = """
    Statement failed, SQLSTATE = HY000
    record not found for user: TMP$2988
    Warning: ALL on ALL is not granted to TMP$2988.
    Statement failed, SQLSTATE = 40001
    deadlock
    -update conflicts with concurrent update
    -concurrent transaction number is 18
    -At block line: 7, col: 9

    Statement failed, SQLSTATE = 40001
    deadlock
    -read conflicts with concurrent update
    -concurrent transaction number is 21
    -At block line: 7, col: 9

    Statement failed, SQLSTATE = 40001
    deadlock
    -read conflicts with concurrent update
    -concurrent transaction number is 24
    -At block line: 7, col: 9
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.expected_stderr = expected_stderr_1
    act_1.execute()
    assert (act_1.clean_stderr == act_1.clean_expected_stderr and
            act_1.clean_stdout == act_1.clean_expected_stdout)

# version: 4.0

substitutions_2 = [('^((?!concurrent transaction number is).)*$', ''),
                   ('[\\-]{0,1}concurrent transaction number is [0-9]+', 'concurrent transaction number is')]

init_script_2 = """
    create table test(id int, x int, constraint test_pk primary key(id) using index test_pk);
    commit;
    insert into test(id, x) values(1, 111);
    commit;
"""

db_2 = db_factory(sql_dialect=3, init=init_script_2)

act_2 = python_act('db_2', substitutions=substitutions_2)

expected_stdout_2 = """
concurrent transaction number is 13
concurrent transaction number is 13
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action, capsys):
    with act_2.db.connect() as con:
        tx1 = con.transaction_manager()
        tx1.begin()
        cur1 = tx1.cursor()
        cur1.execute('update test set x = ? where id = ?', (222, 1))
        # **INSTEAD** of ticket case-1:
        #     set transaction read committed no record_version lock timeout N;
        # -- we start Tx with lock_timeout using custom TPB and try just to **update** record which is locked now
        # (but NOT 'SELECT ...'! It is useless with default value of confign parameter ReadConsistency = 1).
        # Message about concurrent transaction (which holds lock) in any case must appear in exception text.
        # NB: NO_rec_version is USELESS in default FB 4.0 config!
        custom_tpb = TPB(access_mode=TraAccessMode.WRITE,
                         isolation=Isolation.READ_COMMITTED_RECORD_VERSION,
                         lock_timeout=1)
        tx2 = con.transaction_manager(default_tpb=custom_tpb.get_buffer())
        tx2.begin()
        cur2 = tx2.cursor()
        try:
            cur2.execute('update test set x = ? where id = ?', (333, 1))
        except Exception as e:
            print('Exception in cur2:')
            print('-' * 30)
            for x in e.args:
                print(x)
            print('-' * 30)
        finally:
            tx2.commit()
        # This is for ticket case-2:
        #     set transaction read committed lock timeout N;
        #     select * from test with lock;
        custom_tpb.isolation = Isolation.CONCURRENCY
        tx3 = con.transaction_manager(default_tpb=custom_tpb.get_buffer())
        tx3.begin()
        cur3 = tx3.cursor()
        try:
            cur3.execute('select x from test where id = ? with lock', (1,))
            for r in cur3:
                print(r[0])
        except Exception as e:
            print('Exception in cur3:')
            print('-' * 30)
            for x in e.args:
                print(x)
            print('-' * 30)
        finally:
            tx3.commit()
        tx1.commit()
    act_2.expected_stdout = expected_stdout_2
    act_2.stdout = capsys.readouterr().out
    assert act_2.clean_stdout == act_2.clean_expected_stdout
