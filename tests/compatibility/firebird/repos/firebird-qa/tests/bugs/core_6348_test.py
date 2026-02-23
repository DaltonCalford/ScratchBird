#coding:utf-8

"""
ID:          issue-6589
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6589
TITLE:       Wire compression causes freezes
DESCRIPTION:
    Test runs four measurings for following combinations of source blob data and value of WireCompression,
    with comparing RATIOS of loading time and thresholds that was obtained beforehand:
    -------------------------------------------------------------
    |Suitability of| Value of       |CHECK: loading time ratio  |
    |blob data for | WireCompression|when WireCompression was   |
    |compression   | False  |  True |True vs False <= THRESHOLD |
    |--------------|--------|-------|---------------------------|
    |EXCELLENT     |   U1   |   U2  | U2/U1: no more than <UT> ?|
    |--------------|--------|-------|---------------------------|
    |INCOMPRESSIBLE|   V1   |   V2  | V2/V1: no more than <VT> ?|
    -------------------------------------------------------------

    We use GTT in order its content be automatically truncated after each iteration.
    Two kind of blobs are generated on each iteration: 'ideally compressable' or 'absolutely incompressible',
    and temporary file ('tmp_data') is used to store it and then for loading as stream blob into GTT.
    Size of blob can be adjusted via 'DATA_LEN'; but CURRENTLY it seems that it must be NOT LESS then 20 Mb,
    otherwise time of loading can be close to zero, and RATIO between times values will be unreal.

    Test creates custom driver config object ('db_cfg_object') in order to create DPB with appropriate values
    of WireCompression (false true; beside ot that, this DPB contains WireCrypt = Disabled on every iteration).

    After this, we run N_MEASURES measutrings for each of above-mentioned combination, and evaluate MEDIAN value
    of loading time for such serie.
    ::: NB ::: time is measured as difference between psutil.Process(fb_pid).cpu_times() before and after loading.

    Finally, we get two RATIOS between these medians: one for blob that is 'ideally compressable', and second for
    blob which nature is is 'absolutely incompressible'.
    Measuring have been done both on Windows and Linux, for FB 3.0.8, 4.0.1 and 5.0.0.
    Results for FB 4.x and 5.x are very close to each other. 
    These are outcomes for FB 3.0.8 Release and 5.0.0.623:
    ==================
    WINDOWS:
      excellent:  FB 5x: 1.75; 2.25; 2.33; 2.00; 2.67.   FB 308: 1.80; 2.50; 1.80; 2.00
      incompress: FB 5x: 1.67; 1.50; 1.25; 1.75; 1.50.   FB 308: 1.60; 2.00; 1.20; 1.50
    LINUX:
      excellent:  FB 5x: 1.58; 1.77; 1.82; 1.71; 1.60.   FB 308: 2.23; 1.93; 1.93; 2.30
      incompress: FB 5x: 1.71; 1.74; 1.63; 2.04; 1.75.   FB 308: 1.93; 2.15; 2.00; 2.58
    ==================
    As one can see, WireCompression = true leads to increasing of load time approx. for 1.8 ... 2.5.
    Because these data differ slightly, it was decided to set SINGLE threshold all check combinations
    (i.e. the same for <UT> and <VT> in the table that is shown above).
    
JIRA:        CORE-6348
FBTEST:      bugs.core_6348
NOTES:
    [14.08.2022] pzotov
    1. Confirmed problem on 3.0.6.33326: max allowed length of blob (if DB page_size = 8K) = 8174 bytes.
       For values >= 8175 bytes blob loading hangs or takes an unacceptable time.
       On build 3.0.6.33332 no more this problem. Loading of blob of length = 1Gb takes 61952 ms or 52716 ms
       (for wirecompression = false / tru respectively).
    2. Fully re-implemented. No more changes in firebird.conf (custom driver config object and DPB are used).
       No more neither ISQL with datediff(), nor Python timedelta; psutil cpu_times() is used to get exact value
       of time that was spent for loading blob.
       Test duration is about 30...50s, depending of OS and hardware.
       Checked on 5.0.0.623, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux.

    [03.03.2023] pzotov
        Fixed wrong usage of driver_config.register_database() instance, added check for properly selected protocol (only INET must be used).
        Added columns to GTT in order to see in the trace used values of compression_suitability and wire_compression.
        Checked on Windows: 5.0.0.967 SS/CS, 4.0.3.2904 SS/CS, 3.0.11.33665 SS/CS
"""

import os
from pathlib import Path
import psutil

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol


#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

db = db_factory(init = 'recreate global temporary table gtt_test(b blob, compression_suitability varchar(10), wire_compression varchar(10)) on commit delete rows;')

act = python_act('db')

tmp_data = temp_file(filename = 'tmp_6348.dat')

expected_stdout = """
    Median time for loading blob: acceptable.
"""

#  ###########################
#  ###   S E T T I N G S   ###
#  ###########################
#
# Number of measures (how many times we must recreate table and load blob into it):
N_MEASURES = 11

# Length of generated blob:
DATA_LEN = 50 * 1024 * 1024

# max allowed ratio between MEDIANS when WireCompression = True vs False.
# Medians are evalueted for <N_MEASURES> iterations, each value is diff between cpu_times
# when blob was loaed.
# 
MAX_THRESHOLD_FOR_COMPRESS_ON_VS_OFF = 4.0

@pytest.mark.perf_measure
@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_data: Path, capsys):

    srv_cfg = driver_config.register_server(name = 'test_srv_core_6348', config = '')
    blob_load_map = {}
    for compression_suitability in ('excellent', 'incompres'):
        if compression_suitability == 'excellent':
            tmp_data.write_bytes( ('A' * DATA_LEN).encode() )
        else:
            tmp_data.write_bytes( bytearray(os.urandom(DATA_LEN)) )

        for wire_compression in (True, False):
            db_cfg_name = f'tmp_6348_compression_suit_{compression_suitability}__wire_compression_{wire_compression}'
            db_cfg_object = driver_config.register_database(name = db_cfg_name)
            db_cfg_object.server.value = srv_cfg.name # 'test_srv_core_6348'
            # db_cfg_object.protocol.value = None /  XNET /  WNET -- these can not be used in this test!
            db_cfg_object.protocol.value = NetProtocol.INET
            db_cfg_object.database.value = str(act.db.db_path)
            
            db_cfg_object.config.value = f"""
                WireCrypt = Disabled
                WireCompression = {wire_compression}
            """
            blob_load_lst = []
            with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
                prot_name = db_cfg_object.protocol.value.name if db_cfg_object.protocol.value else 'Embedded'
                assert wire_compression == con.info.is_compressed(), f'Protocol: {prot_name} - can not be used to measure wire_compression'
                with con.cursor() as cur:
                    cur.execute('select mon$server_pid, mon$remote_protocol as p from mon$attachments where mon$attachment_id = current_connection')
                    for r in cur:
                        fb_pid, connection_protocol = r
                        assert connection_protocol.startswith('TCP'), f'Invalid connection protocol: {connection_protocol}'

                    for iter in range(0,N_MEASURES):
                        # Load stream blob into table: open file with data and pass file object into INSERT command.
                        # https://firebird-driver.readthedocs.io/en/latest/usage-guide.html?highlight=blob#working-with-blobs
                        with open(tmp_data, 'rb') as blob_file:
                            fb_info_a = psutil.Process(fb_pid).cpu_times()
                            cur.execute("insert into gtt_test(b, compression_suitability, wire_compression) values(?, ?, ?)", (blob_file, compression_suitability, str(wire_compression)) )
                            fb_info_b = psutil.Process(fb_pid).cpu_times()
                            blob_load_lst.append( fb_info_b.user - fb_info_a.user )
                            con.commit()

                blob_load_map[ compression_suitability, con.info.is_compressed() ] = median( blob_load_lst )

    excellent_compress_on_vs_off = blob_load_map[('excellent',True)] / max(0.00001, blob_load_map[('excellent',False)])
    incompres_compress_on_vs_off = blob_load_map[('incompres',True)] / max(0.00001, blob_load_map[('incompres',False)])

    act.expected_stdout = ''
    for var_name in ('excellent_compress_on_vs_off', 'incompres_compress_on_vs_off'):
        msg_prefix  = f'Ratio for {var_name}: '
        act.expected_stdout += msg_prefix + 'OK, acceptable.\n'
        if locals()[var_name] <= MAX_THRESHOLD_FOR_COMPRESS_ON_VS_OFF:
            print(msg_prefix + 'OK, acceptable.')
        else:
            print(msg_prefix + '/* perf_issue_tag */ TOO HIGH: %9.2f -  more than threshold: %6.2f' % ( locals()[var_name], MAX_THRESHOLD_FOR_COMPRESS_ON_VS_OFF ) )

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

