#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8663
TITLE:       Problem with restore time when DB has many indices (possible regression in 6.x)
DESCRIPTION:
    Test uses a pre-prepared database which has one master table ('TMAIN') and 200 details which refer to it.
    In order to be able to evaluate performance, we have to run some action that does not affect on IO but consumes
    valuable CPU time. Call of CRYPT_HASH(<long_string> using SHA512) is used for this. This function is called
    <N_HASH_EVALUATE_COUNT> times with measuring CPU time.
    Then we invoke restore (using services API) and also measure CPU time.
    We repeate these two actions <N_MEASURES> times, so at the end two arrays will be filled: one with CPU-time for
    CRYPT_HASH() and second for restore.
    Medians are evaluated for each array:
        sp_gen_hash_median = median([v for k,v in times_map.items() if k[0] == 'hash_eval'])
        restore_time_median = median([v for k,v in times_map.items() if k[0] == 'restore'])
    If ratio restore_time_median / sp_gen_hash_median NOT greater than some threshold <MAX_RATIO> then test PASSES.
NOTES:
    [24.07.2025] pzotov
    Test can run only on SuperServer / SuperClassic because on Classic server PID changes during restore.
    Several runs showed that medians ratio on 4.x ... 6.x is about 0.8.
    Confirmed poor performance on 6.0.0.1046-1c06452: ratio is ~2.6
    Test duration: 15...20s.
    Checked on 6.0.0.1052; 5.0.3.1684; 4.0.6.3222.
"""
import os
import psutil
import zipfile
import time
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import SrvRestoreFlag, driver_config, connect, NetProtocol, DatabaseError
###########################
###   S E T T I N G S   ###
###########################

# How many times we do measure:
N_MEASURES = 5

# How many iterations must be done for hash evaluation:
N_HASH_EVALUATE_COUNT = 10000

# Max allowed ratio between median values of CPU time measured for UPDATE vs CRYPT_HASH:
MAX_RATIO = 1.2

EXPECTED_MSG = f'acceptable'

db = db_factory()
act = python_act('db')

tmp_fbk = temp_file('tmp_8663.fbk')
tmp_fdb = temp_file('tmp_8663.fdb')
tmp_log = temp_file('tmp_8663.log')

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, tmp_log: Path, capsys):

    if 'classic' in act.vars['server-arch'].lower():
        pytest.skip('Implemented only for SS/SC')
    
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_8663-ods13_0.zip', at='gh_8663.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())

    times_map = {}
    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])
        init_script = """
            create or alter procedure sp_gen_hash (n_cnt int) as
                declare v_hash varbinary(64);
                declare s varchar(32765);
            begin
                s = lpad('', 32765, uuid_to_char(gen_uuid()));
                while (n_cnt > 0) do
                begin
                    v_hash = crypt_hash(s using SHA512);
                    n_cnt = n_cnt - 1;
                end
            end
        """
        con.execute_immediate(init_script)
        con.commit()

        for i in range(0, N_MEASURES):
            try:
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( 'sp_gen_hash', (N_HASH_EVALUATE_COUNT,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()
                times_map[ 'hash_eval', i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)

    restore_failed = 0
    with act.connect_server(user = act.db.user, password = act.db.password) as srv:
        for i in range(0, N_MEASURES):
            tmp_fdb.unlink(missing_ok = True)
            fb_info_init = psutil.Process(fb_pid).cpu_times()
            
            srv.database.restore(database=tmp_fdb, backup=tmp_fbk, verbose = True, flags=SrvRestoreFlag.REPLACE)
            gbak_restore_log = '\n'.join([x.strip() for x in srv.readlines()])

            # ::: NB ::: do NOT invoke cpu_times() before restore log will be obtained via srv.readlines()!
            fb_info_curr = psutil.Process(fb_pid).cpu_times()
            if 'ERROR' in gbak_restore_log: 
                print('Unexpected error during restore:')
                print(gbak_restore_log)
                restore_failed = 1
                break
            else:
                times_map[ 'restore', i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

    if restore_failed: 
        pass
    else:

        sp_gen_hash_median = median([v for k,v in times_map.items() if k[0] == 'hash_eval'])
        restore_time_median = median([v for k,v in times_map.items() if k[0] == 'restore'])
        median_ratio = restore_time_median / sp_gen_hash_median

        print( 'Medians ratio: ' + (EXPECTED_MSG if median_ratio < MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )

        if median_ratio > MAX_RATIO:
            print(f'CPU times for each of {N_MEASURES} measures:')
            for what_measured in ('hash_eval', 'restore', ):
                print(f'{what_measured=}:')
                for p in [v for k,v in times_map.items() if k[0] == what_measured]:
                    print(p)

    act.expected_stdout = f"""
        Medians ratio: {EXPECTED_MSG}
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
