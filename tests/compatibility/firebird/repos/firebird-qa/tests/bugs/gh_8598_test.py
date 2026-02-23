#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8598
TITLE:       Don't fire referential integrity triggers if primary or unique keys haven't changed
DESCRIPTION:
    Test uses a pre-prepared database which has one master table ('TMAIN') and 300 details which refer to it.
    Master table has one record and one column that is not involved in any FK (its name: 'nk').
    In order to be able to evaluate performance, we have to run some action that does not affect on IO but consumes
    valuable CPU time. Call of CRYPT_HASH(<long_string> using SHA512) is used for this. This function is called
    <N_HASH_EVALUATE_COUNT> times with measuring CPU time.
    Then we run <UPDATE_NON_KEY_CNT> times statement 'UPDATE TMAIN SET NK = ? WHERE ...' and also measure CPU time.
    We repeate these two actions <N_MEASURES> times, so at the end two arrays will be filled: one with CPU-time for
    CRYPT_HASH() and second for UPDATE statement.
    Medians are evaluated for each array:
        sp_gen_hash_median = median([v for k,v in times_map.items() if k[0] == 'hash_eval'])
        update_nonk_median = median([v for k,v in times_map.items() if k[0] == 'update_nonk'])
    If ratio update_nonk_median / sp_gen_hash_median NOT greater than some threshold <MAX_RATIO> then test PASSES.
NOTES:
    [22.07.2025] pzotov
    1. Related commits:
        6.x: f6a3e4c81d63827d99fd8688c43460192279960c (25.06.2025) // Front-ported pull request #8600
        5.x: 449449cd2119314be8210f4172c3f0fb5606c555 (25.06.2025) -- within push b5f5ba1a314ec53d583277242bd3e8d8f15b0783
        4.x: cf67735bf8f3f8af47891847317398c10290019d (25.06.2025) -- DID NOT SOLVE PROBLEM
             e6667b99f6bfaa6e5b135cedfbf9f6515897355d (22.07.2025) -- solved (postfix)
       BEFORE fix medians ratio was 22 ... 28. AFTER fix it became 4 ... 7.

    2. Confirmed poor performance on sbnapshots before fix (median_ratio = ~24...26):
        6.0.0.858-cbbbf3b ; 5.0.3.1668-b8e226a ; 4.0.6.3214-e11f62c
        NOTE! Snapshot 4.0.6.3221-ba9bbd0 (13.07.2025) still had *POOR* performance despite that its date >= 25.06.2025
        The problem on 4.x has been solved only since e6667b99.
    3. On 6.x weird problem with restore from test .fbk currently exists: it lasts for more that 3x comparing to 4.x and 5.x

    Checked on Windows: 6.0.0.1050-cee7854 ; 5.0.3.1684-e451f30 ; 4.0.6.3221-e6667b9 (intermediate snapshot)

    [23.07.2025] pzotov
    Added custom driver config otherwise 'unavaliable database' raises on attempt to connect to test DB via 'with connect(...)'.
    After fix #8663 (commit: 9458c3766007ac3696e8c01ed80be96e1098c05f) no more performance problem with restore time
    Checked on 6.0.0.1052-2279f7b.

    [26.07.2025] pzotov
    Increased max allowed ratio between median values (MAX_RATIO) for Linux after several runs: currently this ratio is ~9.5 ... 11.5.
    Test duration: ~35s.
    Checked on 6.0.0.1077
"""
import os
import pytest
import psutil
import zipfile
import time
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag, driver_config, connect, NetProtocol, DatabaseError
###########################
###   S E T T I N G S   ###
###########################

# How many times we do measure:
N_MEASURES = 9

# How many iterations must be done for hash evaluation:
N_HASH_EVALUATE_COUNT = 500

# How many times we do update on-key column in the 'TMAIN' table:
UPDATE_NON_KEY_CNT = 5000

db = db_factory(charset = 'win1251')
act = python_act('db')

tmp_fbk = temp_file('tmp_8598.fbk')
tmp_fdb = temp_file('tmp_8598.fdb')
tmp_log = temp_file('tmp_8598.log')


for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.6')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, tmp_log: Path, capsys):

    # Max allowed ratio between median values of CPU time measured for UPDATE vs CRYPT_HASH:
    MAX_RATIO = 9.0 if os.name == 'nt' else 15
    
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_8598-ods13_0.zip', at='gh_8598.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())
    with act.connect_server(user = act.db.user, password = act.db.password) as srv:
        srv.database.restore(database=tmp_fdb, backup=tmp_fbk, verbose = True, flags=SrvRestoreFlag.REPLACE)
        gbak_restore_log = '\n'.join([x.strip() for x in srv.readlines()])
        with open(tmp_log, 'w') as f:
            f.write(gbak_restore_log)

    if 'ERROR' in gbak_restore_log: 
        print('Unexpected error during restore:')
        print(gbak_restore_log)
    else:
        srv_cfg = driver_config.register_server(name = 'tmp_srv_cfg_8598', config = '')
        db_cfg_name = f'tmp_db_cfg_8598'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)
        db_cfg_object.server.value = srv_cfg.name
        db_cfg_object.protocol.value = NetProtocol.INET
        db_cfg_object.database.value = str(tmp_fdb)

        times_map = {}
        with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
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
                ps = None
                try:

                    fb_info_init = psutil.Process(fb_pid).cpu_times()
                    cur.callproc( 'sp_gen_hash', (N_HASH_EVALUATE_COUNT,) )
                    fb_info_curr = psutil.Process(fb_pid).cpu_times()
                    times_map[ 'hash_eval', i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

                    ps = cur.prepare('update tmain set nk = ? where x0 = 0')
                    fb_info_init = psutil.Process(fb_pid).cpu_times()
                    for k in range(UPDATE_NON_KEY_CNT):
                        cur.execute(ps, (k,))
                    fb_info_curr = psutil.Process(fb_pid).cpu_times()
                    times_map[ 'update_nonk', i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

                except DatabaseError as e:
                    print(e.__str__())
                    print(e.gds_codes)
                finally:
                    if ps:
                        ps.free()

        sp_gen_hash_median = median([v for k,v in times_map.items() if k[0] == 'hash_eval'])
        update_nonk_median = median([v for k,v in times_map.items() if k[0] == 'update_nonk'])
        median_ratio = update_nonk_median / sp_gen_hash_median

        EXPECTED_MSG = f'acceptable, median_ratio less than {MAX_RATIO=}'
        print( 'Medians ratio: ' + (EXPECTED_MSG if median_ratio < MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )

        if median_ratio > MAX_RATIO:
            print(f'CPU times for each of {N_MEASURES} measures:')
            for what_measured in ('hash_eval', 'update_nonk', ):
                print(f'{what_measured=}:')
                for p in [v for k,v in times_map.items() if k[0] == what_measured]:
                    print(p)

    act.expected_stdout = f"""
        Medians ratio: {EXPECTED_MSG}
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
