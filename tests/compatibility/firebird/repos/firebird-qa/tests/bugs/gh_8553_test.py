#coding:utf-8

"""
ID:          issue-8553
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8553
TITLE:       Get the modification time of a config file with a higher precision to fix cases when it's not reloaded after modification/replacement
DESCRIPTION:
    Test assumes that QA account has write access to the $FB_HOME directory: file databases.conf will be temporary overwritten here.
    Temporary .fdb file is created (see 'tmp_fdb') and its content will be overwritten by test DB.
    We preserve original content of databases.conf (see 'tmp_cfg_original') and run try/except/finally block with restore this file at final.
    Within the 'try' section we overwrite databases.conf by creating section with appropriate alias that points to <tmp_fdb> and has parameter
    'DefaultDbCachePages'.
    When first connection to <tmp_fdb> is established, we save values of these parameters (see 'changed_params_lst').
    After this connect finished, we change AGAIN content of databases.conf by writing different values for these parameters and make second connection.
    When second connection to <tmp_fdb> is established, we increment 'changed_params_lst' (again by adding values of these parameters).
    After second connection finished, we must have list with TWO results of query which obtained parameters, and these results MUST DIFFER.
    In other words, the Set() object constructed from this list must have length = 2 - and test verifies that.
    Also, test verifies that time between connections less than 1 second (see 'MAX_MS_DIFF').
NOTES:
    [25.09.2025] pzotov
    1. ### CAUTION ### Weird outcome may occur on any snapshot before this ticket was fixed: in approx. 15-25% of cases test will PASSED there.
    2. Test does NOT use COnfigManager fixture: it appears that after some repeating checks FB can not to properly parse databases.conf and
       attempt to make connection to employee database will fail with:
       SQLSTATE = 08001 / I/O error during "CreateFile (open)" operation for file "employee" / -Error while trying to open file
       (but this file for sure presents and its alias remains unchanged in the restored databases.conf).
       THIS MUST BE INVESTIGATED.

    Confirmed problem on: 6.0.0.1280; 5.0.4.1714; 4.0.7.3234; 3.0.14.33825 (second connection could seen 'old' config parameters).
    Checked on 6.0.0.1282; 5.0.4.1715; 4.0.7.3235; 3.0.14.33826.
"""

from pathlib import Path
import locale
import shutil
from datetime import datetime
import time

import pytest
from firebird.qa import *

# Max timediff in milliseconds between 1st and 2nd connections were established.
# If actual time greater than this value then results can not be considered as reliable
# (warning must be issued in this case).
##################
MAX_MS_DIFF = 1000
##################

db = db_factory(init = 'create sequence g;')
act = python_act('db')
tmp_fdb = temp_file('tmp_8553.fdb')
tmp_cfg_original = temp_file('tmp_8553_dbconf.bak')

@pytest.mark.version('>=3.0.14')
def test_1(act: Action, tmp_fdb: Path, tmp_cfg_original: Path, capsys):

    if act.vars['server-arch'] != 'SuperServer':
        pytest.skip("Applies only to SuperServer")

    updated_db_conf_1 = f"""
        tmp_gh_8553_alias = {tmp_fdb}
        {{
            DefaultDbCachePages = 11111
            LockHashSlots = 9311
        }}
    """

    # Before fix these values might not be seen (but not always!).
    # After fix these values always *must* be seen after reconnect:
    #
    updated_db_conf_2 = f"""
        tmp_gh_8553_alias = {tmp_fdb}
        {{
            DefaultDbCachePages = 22222
            LockHashSlots = 9377
        }}
    """

    shutil.copy2(act.db.db_path, tmp_fdb)
    shutil.copy2(act.vars['home-dir'] / 'databases.conf', tmp_cfg_original)

    test_sql = f"""
        set list on;
        set count on;
        set bail on;
        connect 'localhost:tmp_gh_8553_alias' user {act.db.user} password '{act.db.password}';
    """
    if act.is_version('<4'):
        test_sql += """
            select
                gen_id(g,1) as mutable_attach_no
                ,current_timestamp as mutable_timestamp
                ,m.mon$database_name as db_name
                ,m.mon$page_buffers as changed_page_buffers
            from mon$database m
            ;
        """
    else:
        test_sql += """
            select
                gen_id(g,1) as mutable_attach_no
                ,current_timestamp as mutable_timestamp
                ,m.mon$database_name as db_name
                ,m.mon$page_buffers as changed_page_buffers
                ,g.rdb$config_value as changed_hash_slots -- resuired FB 4.x+
            from mon$database m
            cross join rdb$config g where g.rdb$config_name = 'LockHashSlots'
            ;
        """

    out_list = []
    for i, x_conf in enumerate((updated_db_conf_1, updated_db_conf_2)):
        try:
            with open(act.vars['home-dir'] / 'databases.conf', 'w') as f:
                f.write(x_conf)
            act.isql(switches = ['-q'], input = test_sql, credentials = False, connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())
            out_list.append(act.stdout)
        except Error as e:
            print(e)
        finally:
            shutil.copy2(tmp_cfg_original, act.vars['home-dir'] / 'databases.conf')

    changed_params_set = set()
    changed_params_lst = []
    dto_lst = []
    for p in out_list:
        # MUTABLE_TIMESTAMP  2025-09-25 20:00:50.8920 Europe/Moscow
        attach_dts = [' '.join(s.split()[1:3]) for s in p.splitlines() if s.startswith('MUTABLE_TIMESTAMP')][0]
        dto_lst.append( datetime.strptime(attach_dts, '%Y-%m-%d %H:%M:%S.%f') )
        p_params = []
        for line in p.splitlines():
            if not line.strip() or line.upper().startswith('mutable_'.upper()):
                pass
            else:
                p_params.append(line,)
        changed_params_set.add(tuple(p_params))
        changed_params_lst.append(p_params)

    dd = dto_lst[1] - dto_lst[0]
    reconnect_ms = dd.seconds*1000 + dd.microseconds//1000

    if reconnect_ms >= MAX_MS_DIFF:
        print('### WARNING ### Results may be unreliable: test runs TOO SLOW.')
        print('Timediff between connections, reconnect_ms: {reconnect_ms} - greater than {MAX_MS_DIFF=}')
        print(f'{dto_lst=}, {reconnect_ms=}')
    
    EXPECTED_MSG = 'Changes in the configuration have been detected after re-connect.'
    if len(changed_params_set) == 2:
        print(EXPECTED_MSG)
    else:
        print('### ERROR ### CHANGES IN THE CONFIGURATION HAVE NOT BEEN DETECTED AFTER RE-CONNECT.')
        for i,p in enumerate(changed_params_lst):
            print(f'Configuration visible %s:' % ('to the first connection' if i==0 else 'after re-connect'))
            for line in p:
                print(line)
        print(f'Timediff between connection, reconnect_ms: {reconnect_ms}. Count of distinguishable configurations: {len(changed_params_set)}')

    act.expected_stdout = f"""
        {EXPECTED_MSG}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
