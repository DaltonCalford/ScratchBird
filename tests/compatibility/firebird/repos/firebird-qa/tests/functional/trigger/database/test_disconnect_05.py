#coding:utf-8

"""
ID:          trigger.database.trigger_on_disconnect_05
TITLE:       ON DISCONNECT trigger and OnDisconnectTriggerTimeout expiration
DESCRIPTION: 
    Improvement was introduced 06-apr-2023, commits:
        https://github.com/FirebirdSQL/firebird/commit/040db0e05a4d880296c451cdc865077d9d4f2619
        ("Add OnDisconnectTriggerTimeout parameter to firebird.conf")
        https://github.com/FirebirdSQL/firebird/commit/fc85f89be486d3bce16160a798907dfc35bead52
        (Print ON DISCONNECT trigger name with a stack trace to firebird.log when it is cancelled by a timeout)

    Test uses database that was added to $QA_ROOT/files/qa-databases.conf (see variable 'REQUIRED_ALIAS').
    This alias contains parameter OnDisconnectTriggerTimeout with some small value (currently it is 2),
    thus SYSDBA can obtain it via SQL query to RDB$CONFIG table. This value ('trg_timeout') is saved
    into the table 'cfg' which can be accessed by anyone who was granted PUBLIC role.
    Value of timeout for ON DISCONNECT trigger is used further in its PSQL code.
    
    When connection is to be finished, trigger will fire and fall in some kind of 'heavy code' (just trivial
    loop with huge number of iterations with query to rdb$database).

    This trigger will work valuable time. But presense of parameter OnDisconnectTriggerTimeout *must* limit
    duration of this job and trigger execution will be forcibly terminated. If this functionality will be
    broken by some way, exception 'exc_timeout_greater_than_configured' will raise.

    Also, test creates table 'detach_audit' which is fulfilled from ON DISCONNECT trigger if current user
    not SYSDBA. We create non-privileged user ('tmp_worker') and make connection to database. 
    When user makes connection, it will be alive only for <SESSION_IDLE_TIMEOUT> seconds, and will be closed
    after this time expired. This *must* cause firing trigger with adding record to 'detach_audit' table.

    Before making connection as user 'TMP_WORKER', we:
        * get content of firebird.log
        * launch trace
    After tmp_worker completes its work (running ISQL with single command: 'quit'), we get again content of
    firebird.log for further comparison.

    Finally, we check that:
        * trace contains messages about failed trigger, cancelled operation, timeout expiration and call stack;
        * difference in firebird.log contains messages similar to trace
        * table 'detach_audit' has ONE record (containing name of non-priv user).
NOTES:
    [28.02.2023] pzotov
    ::: NB :::
    Call of act.trace() must include parameter 'database' with value that points to the *file* name of database,
    which otherwise will be defined *wrongly* as: database=%[\\/]REQUIRED_ALIAS.
    Name of database *file* is: tmp_fdb.name

    Checked on 5.0.0.467 (07-apr-2022), 5.0.0.964 SS/CS - all OK.
"""

import pytest
from firebird.qa import *
import re
import locale
from pathlib import Path
from difflib import unified_diff
import time

# Name of alias for self-security DB in the QA_root/files/qa-databases.conf.
# This file must be copied manually to each testing FB homw folder, with replacing
# databases.conf there:
#
REQUIRED_ALIAS = 'tmp_trg_disconn_timeout_alias'

tmp_worker = user_factory('db', name='tmp_worker', password='123')
db = db_factory()
substitutions = [ ('.* FAILED EXECUTE_TRIGGER_FINISH', 'FAILED EXECUTE_TRIGGER_FINISH'),
                  ('.* ERROR AT purge_attachment', 'ERROR AT purge_attachment'),
                  ('.* line: \\d+, col: \\d+', '')
                ]

act = python_act('db', substitutions = substitutions)

@pytest.mark.trace
@pytest.mark.version('>=5.0')
def test_1(act: Action, tmp_worker: User, capsys):


    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_7046_alias = $(dir_sampleDb)/qa/tmp_gh_7046.fdb
                # - then we extract filename: 'tmp_gh_7046.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    init_sql  = f"""
        set list on;
        set bail on;
        create database '{REQUIRED_ALIAS}' user {act.db.user};
        create exception exc_not_found_config_param 'Parameter "@1" has incorrect value or was not found in the RDB$CONFIG table';

        recreate table detach_audit(
            id int generated always as identity
            ,dts timestamp default 'now'
            ,who varchar(31) default current_user
            ,memo varchar(50)
        );
        recreate table cfg(trg_timeout int);
        commit;
        grant select, insert on detach_audit to public;
        grant select on cfg to public;
        commit;

        insert into cfg(trg_timeout) select rdb$config_value from rdb$config where upper(rdb$config_name) = upper('OnDisconnectTriggerTimeout');
        commit;

        set term ^;
        create procedure sp_test(a int) as
        begin
            --- nop ---
        end
        ^
        commit
        ^
        create trigger trg_disconnect on disconnect as
            declare n int = 100000000;
            declare v int;
            declare trg_timeout int = -1;
            declare t1 timestamp;
        begin
            if ( current_user != '{act.db.user}' ) then
            begin
                execute statement 'select trg_timeout from cfg' into trg_timeout;
                execute procedure sp_test(trg_timeout);

                if ( coalesce(trg_timeout,0) <= 0 ) then
                    exception exc_not_found_config_param using ('OnDisconnectTriggerTimeout');

                execute statement ('insert into detach_audit(memo) values(''point-1'')')
                with autonomous transaction
                ;
                t1 = 'now';
                while ( n > 0 ) do
                begin
                    select 1 from rdb$database into v;
                    n = n - 1;
                    if ( datediff(second from t1 to cast('now' as timestamp)) > 1 * trg_timeout ) then
                    begin
                        execute statement ('insert into detach_audit(memo) values(''point-2'')')
                        with autonomous transaction
                        ;
                        leave;
                    end
                end
                execute statement ('insert into detach_audit(memo) values(''point-3'')');
            end
        end
        ^
        set term ;^
        commit;
        -- select rdb$config_value from rdb$config where upper(rdb$config_name) = upper('OnDisconnectTriggerTimeout');
    """
    act.expected_stdout = ''
    act.isql(switches=['-q'], input = init_sql, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
    
    # ----------------------------------------------------------------------------------------------
    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_init = srv.readlines()
    # ----------------------------------------------------------------------------------------------

    trace_cfg_items = [
        'log_connections = true',
        'log_transactions = true',
        'time_threshold = 0',
        'log_errors = true',
        'log_statement_finish = true',
        'log_trigger_finish = true',
        'max_sql_length = 32768',
    ]

    with act.trace(database = tmp_fdb.name, db_events = trace_cfg_items, encoding=locale.getpreferredencoding()):

        # database=%[\\/]tmp_disconn_trg_timeout.fdb
        # { 
        #   <param>  = <value>
        # }

        test_sql = f"""
            connect 'localhost:{tmp_fdb}' user {tmp_worker.name} password '{tmp_worker.password}';
            quit;
        """
        act.isql(switches = ['-q'], input = test_sql, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    # ----------------------------------------------------------------------------------------------

    allowed_patterns = \
    (
         re.escape("insert into detach_audit(memo) values(")
        ,re.escape(') FAILED EXECUTE_TRIGGER_FINISH')
        ,re.escape(') ERROR AT purge_attachment')
        ,'335544794 : operation was cancelled'
        ,'335545127 : Config level timeout expired'
        ,"335544842 : At trigger 'TRG_DISCONNECT' line: \\d+, col: \\d+"
    )
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    for line in act.trace_log:
        if line.strip():
            if act.match_any(line.strip(), allowed_patterns):
                print(line.strip())
        # print(line)

    expected_trace_log = """
        insert into detach_audit(memo) values('point-1')
        2023-02-28T17:10:50.5590 (30436:00000000048F0040) FAILED EXECUTE_TRIGGER_FINISH
        2023-02-28T17:10:50.5590 (30436:00000000048F0040) ERROR AT purge_attachment
        335544794 : operation was cancelled
        335545127 : Config level timeout expired.
        335544842 : At trigger 'TRG_DISCONNECT' line: 121, col: 121
    """
    act.expected_stdout = expected_trace_log
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # ----------------------------------------------------------------------------------------------

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_curr = srv.readlines()


    fb_log_diff_patterns = \
    (
         'Error at disconnect'
        ,'operation was cancelled'
        ,'Config level timeout expired'
        ,"At trigger 'TRG_DISCONNECT' line: \\d+, col: \\d+"
    )
    fb_log_diff_patterns = [ re.compile(p, re.IGNORECASE) for p in fb_log_diff_patterns ]
       
    for line in unified_diff(fb_log_init, fb_log_curr):
        if line.startswith('+'):
            if act.match_any(line[1:].strip(), fb_log_diff_patterns):
                print(line[1:].strip())

    expected_log_diff = """
        Error at disconnect:
        operation was cancelled
        Config level timeout expired.
        At trigger 'TRG_DISCONNECT' line: 23, col: 21    
    """
    act.expected_stdout = expected_log_diff
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

    # ----------------------------------------------------------------------------------------------

    test_sql = f"""
        connect 'localhost:{REQUIRED_ALIAS}' user {act.db.user} password '{act.db.password}';
        set count on;
        set list on;
        select id,who,memo from detach_audit;
        quit;
    """
    expected_final_chk = """
        ID                              1
        WHO                             TMP_WORKER
        MEMO                            point-1
        Records affected: 1
    """

    act.expected_stdout = expected_final_chk
    act.isql(switches=['-q'], input = test_sql, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
