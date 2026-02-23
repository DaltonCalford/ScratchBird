#coding:utf-8

"""
ID:          issue-8673
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8673
TITLE:       Error unable to allocate memory from operating system
DESCRIPTION:
    Test operates with blob which size is slightly greater than limit for inline blobs (64K).
    We get value of resident memory for PID of server process, then gather <MEASURES_COUNT> times
    content of blob field and get value of same kind of memory again.
    Ratio between current and initial values of memory_info().rss must be less than <MAX_RATIO>.
NOTES:
    [29.07.2025] pzotov
    Confirmed memory growth on 6.0.0.1092-daed3df, 5.0.3.1686-1f2fcff: memory ratio is greater than 2.1
    After fix this ratio is 1.01 ... 1.03
    Checked on Windows 6.0.0.1092-8bb8209, 5.0.3.1689-d53bb2e
"""
import time
import psutil

import pytest
from firebird.qa import *

###########################
###   s e t t i n g s   ###
###########################
BLOB_LEN = 65999
MEASURES_COUNT = 7500
MAX_RATIO = 1.05

db = db_factory()
act = python_act('db')
tmp_blob_file = temp_file('tmp_blob_8673.dat')

#--------------------------------------------------------------------
def get_server_pid(con):
    with con.cursor() as cur:
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])
    return fb_pid
#--------------------------------------------------------------------

@pytest.mark.perf_measure
@pytest.mark.version('>=5.0.3')
def test_1(act: Action, capsys):

    init_script = f"""
        recreate table test (
            id int primary key,
            blob_fld blob
        );

        set term ^;
        execute block as
            declare i integer = 1;
            declare b blob = '';
        begin
            while (i <= {BLOB_LEN}) do
            begin
                b = blob_append(b, uuid_to_char(gen_uuid()));
                i = i + 36;
            end
            insert into test(id, blob_fld) values(1, :b);
        end
        ^
        set term ;^
        commit;
    """
    act.isql(switches=['-q'], input = init_script, combine_output = True)
    assert act.clean_stdout == '', f'Initial script FAILED, {act.clean_stdout=}'
    act.reset()

    with act.db.connect() as con:
        server_process = psutil.Process(get_server_pid(con))
        srv_memo_rss_init = int(server_process.memory_info().rss / 1024)
        srv_memo_vms_init = int(server_process.memory_info().vms / 1024)

        cur = con.cursor()
        for k in range(MEASURES_COUNT):
            cur.stream_blobs.append('BLOB_FLD')
            cur.execute('select blob_fld from test')
            blob_reader = cur.fetchone()[0]
            b_data_in_file = blob_reader.read()
            blob_reader.close()

        srv_memo_rss_curr = int(server_process.memory_info().rss / 1024)
        srv_memo_vms_curr = int(server_process.memory_info().vms / 1024)

    memo_ratio = srv_memo_rss_curr / srv_memo_rss_init

    SUCCESS_MSG = 'Ratio between memory values measured before and after loop: acceptable'
    if memo_ratio < MAX_RATIO:
        print(SUCCESS_MSG)
    else:
        print( 'Ratio: /* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:.2f}'.format(memo_ratio), '{:.2f}'.format(MAX_RATIO) ) )

    act.expected_stdout = SUCCESS_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
