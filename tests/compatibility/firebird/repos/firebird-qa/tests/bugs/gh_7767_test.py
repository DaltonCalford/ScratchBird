#coding:utf-8

"""
ID:          issue-7767
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7767
TITLE:       Slow drop trigger command execution under FB5.0
DESCRIPTION:
    The issued problem can NOT be stably reproduced if we compare time ratio between 'DROP TRIGGER' vs 'DROP PROCEDURE' statements.
    ratio between execution time differed for too small value (e.g. 7.2 before fix and 6.9 after it).

    But regression can be noted if we check ratio between CPU time spent for 'DROP TRIGGER' and some code that does not relate
    to any DB operations and makes some evaluation. Such code can be single call to CRYPT_HASH function (doing in loop many times).
    This function must be called EVAL_CRYPT_HASH_COUNT times.
    Result of evaluating of CRYPT_HASH is stored in var. 'eval_crypt_hash_time' and serves further as "etalone" value.

    Test database is initialized by creation of PSQL_OBJECTS_COUNT triggers and is copied to backup FDB (see 'tmp_fdb').
    Then we call 'DROP TRIGGER' using 'for select ... from rdb$triggers' cursor (so their count is also PSQL_OBJECTS_COUNT).
    We repeat this in loop for MEASURES_COUNT iterations, doing restore from DB copy before every iteration (copy 'tmp_fdb' to act.db).
    Median of ratios between CPU times obtained in this loop and eval_crypt_hash_time must be less than MAX_RATIO.
    Duration is measured as difference between psutil.Process(fb_pid).cpu_times() counters.
NOTES:
    [14.08.2024] pzotov
    Problem did exist in FB 5.x until commit "Fix #7759 - Routine calling overhead increased by factor 6 vs Firebird 4.0.0."
    https://github.com/FirebirdSQL/firebird/commit/d621ffbe0cf2d43e13480628992180c28a5044fe (03-oct-2023 13:32).
    Before this commit (up to 5.0.0.1236) median of ratios was more than 6.5.
    After fix it was reduced to ~3.5 ... 4.0 (5.0.0.1237 and above).
    This ratio seems to be same on Windows and Linux.

    Built-in function CRYPT_HASH appeared in 4.0.0.2180, 27-aug-2020, commit:
    https://github.com/FirebirdSQL/firebird/commit/e9f3eb360db41ddff27fa419b908876be0d2daa5
    ("Moved cryptographic hashes to separate function crypt_hash(), crc32 - into function hash()")

    Test duration time: about 50s.
    Checked on 6.0.0.436, 5.0.2.1478, 4.0.6.3142 (all SS/CS; both Windows and Linux).
"""
import shutil
from pathlib import Path
import psutil
import pytest
from firebird.qa import *
import time

###########################
###   S E T T I N G S   ###
###########################

# How many times to generate crypt_hash:
EVAL_CRYPT_HASH_COUNT=5000

# How many times we call procedures:
MEASURES_COUNT = 11

# How many procedures and triggers must be created:
PSQL_OBJECTS_COUNT = 500

# Maximal value for ratio between maximal and minimal medians
#
MAX_RATIO = 6
#############

init_sql = """
    set bail on;
    alter database set linger to 0;
    create sequence g;
    create table test(id int);
    commit;
    set term ^;
"""
init_sql = '\n'.join(
                        ( init_sql
                          ,'\n'.join( [ f'create trigger tg_{i} for test before insert as declare v int; begin v = gen_id(g,1); end ^' for i in range(PSQL_OBJECTS_COUNT) ] )
                          ,'^ set term ;^'
                          ,'commit;'
                        )
                    )

db = db_factory(init = init_sql)
act = python_act('db')

tmp_fdb = temp_file('tmp_gh_7767_copy.tmp')

expected_stdout = """
    Medians ratio: acceptable
"""

eval_crypt_code = f"""
    execute block as
        declare v_hash varbinary(64);
        declare n int = {EVAL_CRYPT_HASH_COUNT};
    begin
        while (n > 0) do begin
            v_hash = crypt_hash(lpad('', 32765, uuid_to_char(gen_uuid())) using SHA512);
            n = n - 1;
        end
    end
"""

drop_trg_code = """
    execute block as
        declare trg_drop type of column rdb$triggers.rdb$trigger_name;
    begin
        for select 'DROP TRIGGER '||trim(rdb$trigger_name)
        from rdb$triggers
        where rdb$system_flag=0
        into :trg_drop do
        begin
        in autonomous transaction do
            begin
                execute statement :trg_drop;
            end
        end
    end
"""

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None

#--------------------------------------------------------------------

def get_server_pid(con):
    with con.cursor() as cur:
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])
    return fb_pid

#--------------------------------------------------------------------

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.0')
def test_1(act: Action, tmp_fdb: Path, capsys):
    
    shutil.copy2(act.db.db_path, tmp_fdb)

    with act.db.connect() as con:
        fb_pid = get_server_pid(con)
        fb_info_init = psutil.Process(fb_pid).cpu_times()
        con.execute_immediate( eval_crypt_code )
        fb_info_curr = psutil.Process(fb_pid).cpu_times()
        eval_crypt_hash_time  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

    ddl_time = {}
    for iter in range(MEASURES_COUNT):

        with act.db.connect() as con:
            fb_pid = get_server_pid(con)
            fb_info_init = psutil.Process(fb_pid).cpu_times()
            con.execute_immediate( drop_trg_code )
            fb_info_curr = psutil.Process(fb_pid).cpu_times()
            ddl_time[ 'tg', iter ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

        # Quick jump back to database with PSQL_OBJECTS_COUNT triggers that we made on init phase:
        shutil.copy2(tmp_fdb, act.db.db_path)

    ratios = [ ddl_time['tg',iter] / eval_crypt_hash_time for iter in range(MEASURES_COUNT) ]
    median_ratio = median(ratios)

    SUCCESS_MSG = 'Medians ratio: acceptable'
    if median_ratio < MAX_RATIO:
        print(SUCCESS_MSG)
    else:
        print( 'Medians ratio: /* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:.2f}'.format(median_ratio), '{:.2f}'.format(MAX_RATIO) ) )
        print('ratios:',['{:.2f}'.format(r) for r in ratios])
        print('CPU times:')
        for k,v in ddl_time.items():
            print(k,':::','{:.2f}'.format(v))

    act.expected_stdout = SUCCESS_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
