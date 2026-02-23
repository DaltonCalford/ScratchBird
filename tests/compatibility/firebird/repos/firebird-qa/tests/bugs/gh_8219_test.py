#coding:utf-8

"""
ID:          issue-8219
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8219
TITLE:       Database creation in 3.0.12, 4.0.5 and 5.0.1 slower than in previous releases
DESCRIPTION:
    We can estimate perfomance by comparison of time that is spent to create DB vs result of some crypt function.
    Function crypt_hash(<string> using SHA512) has been selected for that because of notable CPU consumation.
    Stored procedured SP_GEN_HASH is created for evaluation of crypt hash, it will run loop for N_HASH_EVALUATE_COUNT times.
    Duration for each measure is difference between psutil.Process(fb_pid).cpu_times() counters.
    We do <N_MEASURES> times call of SP and create_database(), with adding results to map.
    Finally, we get ratio between medians of these measures (see 'median_ratio')
   
    Test is considered as passed if median_ratio less than threshold <MAX_RATIO>.
NOTES:
    [05.09.2024] pzotov.
    1. Confirmed problem on snapshotrs before 20-aug-2024.
       Medians ratio on Windows:
           1. Before fix:
               6.0.0.423:  0.39;   6.0.0.436:  0.39;   6.0.0.437:  0.35;
               5.0.1.1464: 0.42;   5.0.1.1469: 0.39;   5.0.1.1479: 0.35
               4.0.5.3136: 0.42;   4.0.6.3142: 0.39
           2. After fix ratio reduced to ~0.25:
               6.0.0.438:  0.21;   6.0.0.442:  0.21;   6.0.0.438:  0.21;   6.0.0.442: 0.21;  6.0.0.450: 0.24
               5.0.2.1481: 0.25;   5.0.2.1482: 0.21;   5.0.2.1493: 0.22
               4.0.6.3144: 0.25;   4.0.6.3149: 0.29

       Medians ratio on Windows:
           1. Before fix:
               6.0.0.397-c734c96:  0.48;   6.0.0.438-088b529:  0.49
           2. After fix ratio reduced to ~0.25:
               6.0.0.441-75042b5:  0.23
               5.0.2.1481-fc71044: 0.24
               4.0.6.3144-5a3b718: 0.27
    2. Test DB must NOT have charset = utf8, otherwise 'implementation limit exceeded' will raise; win1251 was selected for work.
    3. Test can be used only for ServerMode = Super or SuperClassic
       (because in CS a new process is made and we have no value of cpu_times() *before* DB creation).
"""
import os
import psutil
import pytest
from firebird.qa import *
from firebird.driver import driver_config, create_database, NetProtocol
from pathlib import Path

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

###########################
###   S E T T I N G S   ###
###########################

# How many times we create databases
N_MEASURES = 31

# How many iterations must be done for hash evaluation:
N_HASH_EVALUATE_COUNT = 3000

# Maximal value for ratio between maximal and minimal medians
#
#############################################
MAX_RATIO = 0.30 if os.name == 'nt' else 0.33
#############################################

init_script = \
f'''
    set term ^;
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
    ^
    commit
    ^
'''

db = db_factory(init = init_script, charset = 'win1251')
act = python_act('db')
tmp_fdb = temp_file('tmp_gh_8219.tmp')

expected_stdout = """
    Medians ratio: acceptable
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.5')
def test_1(act: Action, tmp_fdb: Path, capsys):

    if act.vars['server-arch'].lower() == 'classic':
        pytest.skip('Can be used only for SS / SC.')
    
    srv_cfg = driver_config.register_server(name = 'test_srv_gh_8219', config = '')

    db_cfg_name = 'tmp_8219'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.protocol.value = NetProtocol.INET
    db_cfg_object.database.value = str(tmp_fdb)

    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        times_map = {}
        for i in range(0, N_MEASURES):
            fb_info_init = psutil.Process(fb_pid).cpu_times()
            cur.callproc( 'sp_gen_hash', (N_HASH_EVALUATE_COUNT,) )
            fb_info_curr = psutil.Process(fb_pid).cpu_times()
            times_map[ 'hash_eval', i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

            fb_info_init = psutil.Process(fb_pid).cpu_times()
            with create_database(db_cfg_name, user = act.db.user, password = act.db.password, overwrite = True) as dbc:
                pass
            fb_info_curr = psutil.Process(fb_pid).cpu_times()
            times_map[ 'db_create', i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)


    sp_gen_hash_median = median([v for k,v in times_map.items() if k[0] == 'hash_eval'])
    sp_db_create_median = median([v for k,v in times_map.items() if k[0] == 'db_create'])

    median_ratio = sp_db_create_median / sp_gen_hash_median

    print( 'Medians ratio: ' + ('acceptable' if median_ratio <= MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )
    if median_ratio > MAX_RATIO:
        print(f'CPU times for each of {N_MEASURES} measures:')
        for sp_name in ('hash_eval', 'db_create', ):
            print(f'{sp_name=}:')
            for p in [v for k,v in times_map.items() if k[0] == sp_name]:
                print(p)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
