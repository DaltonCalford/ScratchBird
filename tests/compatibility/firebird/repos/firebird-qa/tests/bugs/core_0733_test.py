#coding:utf-8

"""
ID:          issue-1108
ISSUE:       1108
TITLE:       Compress Data over the Network
DESCRIPTION:
    Test creates two tables with similar DDL (single varchar column) and fills data into them.
    First table ('t_common_text') gets textual data which can be compressed in extremely high degree.
    Second table ('t_binary_data') will store GUID-data which is obviously incompressable.
    Then we perform <N_MEASURES> runs with selecting data from each of these tables, and we store
    result of *deltas* of CPU user time that was 'captured' before and after cursor fetch all rows.
                                                                                     
    Within each measurement we gather info TWO times, by changing WireCompression =  true vs false
    (this is client-side parameter and it must be defined in DPB; see 'db_cfg_object' variable).

    Results of <N_MEASURES> for each value of WireCompression and each source (t_common_text vs t_binary_data)
    are accumulated in the lists, and MEDIAN (of that CPU time values) is evaluated after all serie completed.

    Following ratios are compared in this test with apropriate 'thresholds':
    1) how CPU UserTime depends on WireCompression if we send 'IDEALLY COMPRESSABLE' textual data over network.
       If WireCompression is ON (and actually works) then CPU time must be GREATER then if WireCompression = OFF.
       Minimal threshold for that case is tuned by 'MIN_CPU_RATIO_TXT_WCOMPR_ON_OFF' setting;
    2) same as "1", but when we send 'ABSOLUTELY IMCOMPRESSABLE' binary data over network.
       Minimal threshold for that case is tuned by 'MIN_CPU_RATIO_BIN_WCOMPR_ON_OFF' setting;
    3) how CPU userTime depends on NATURE of sending data ('ideally compressable' vs 'absolutely imcompressable')
       if WireCompression is OFF. This comparison is not mandatory for this test purpose, but it can be useful
       for estimation of how record-level compression works. 
       Ideally compresable text occupies much less data pages this CPU usertime for processing must be LESS then
       for imcompressible binary data.
       Maximal ratio between them must be more than 1, see setting 'MAX_CPU_RATIO_TXT2BIN_WCOMPR_OFF'.

    We have to compare RATIOS between these CPU time medians with some statistics which was collected beforhand.
    Ten runs was done for Windows and Linux, values of thresholds see in the source below.

    See also tests for:
      CORE-5536 - checks that field mon$wire_compressed actually exists in MON$ATTACHMENTS table;
      CORE-5913 - checks that built-in rdb$get_context('SYSTEM','WIRE_ENCRYPTED') is avaliable;
NOTES:
    [09.11.2021] pcisar
    This test was fragile from start, usualy lefts behind resources and requires
    temporary changes to firebird.conf on run-time. It's questionable whether
    wire-compression should be tested at all.

    [04.08.2022] pzotov
    Fully re-implemented (see description). No more datediff() for time measurement, CPU UserTime is analyzed.
    Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux.

    [03.03.2023] pzotov
        Fixed wrong usage of driver_config.register_database() instance, added check for properly selected protocol (only INET must be used).
        Added columns to GTT in order to see in the trace used values of compression_suitability and wire_compression.
        Checked on Windows: 5.0.0.967 SS/CS, 4.0.3.2904 SS/CS, 3.0.11.33665 SS/CS.

    CAUTION.
    DO NOT set DATA_WIDTH less than 1'500 and N_ROWS less then 10'000 
    otherwise ratios can be unreal because of CPU UserTime = 0.

JIRA:        CORE-733
FBTEST:      bugs.core_0733
"""
import psutil
import platform
from collections import defaultdict

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol
#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

N_MEASURES = 9
DATA_WIDTH = 10000 # 1500 # 8200 #1600
N_ROWS = 15000

# MINIMAL ratio between CPU user time when WireCompression = true vs false
# and we operate with 'IDEALLY COMPRESSABLE' (textual) data.
# Windows: 10.000; 13.330; 10.250; 14.000;  8.000;  9.750;  8.400; 13.667; 10.000 // 4.0.1
#           8.600;  8.800;  8.800;  7.167; 11.000;  8.600;  8.800; 15.000;  7.167 // 3.0.8
# Linux:    7.538;  6.733;  6.266;  6.125;  7.692;  7.538;  8.083;  7.143;  8.083 // 4.0.1
#           7.384;  7.071;  6.999;  6.733;  6.714;  8.083;  6.467;  7.308;  7.538 // 3.0.8
MIN_CPU_RATIO_TXT_WCOMPR_ON_OFF = 5 if platform.system() == 'Linux' else 6


# MINIMAL ratio between CPU user time when WireCompression = true vs false
# and we operate with 'ABSOLUTELY IMCOMPRESSIBLE' data.
# Windows: 5.750; 9.000; 6.286; 6.285; 6.286; 7.167; 5.500; 6.286; 6.285  // 4.0.1
#          4.909; 3.714; 3.643; 4.417  3.000; 4.384; 3.600; 3.533; 4.500  // 3.0.8
# Linux:   5.158; 6.466; 6.333; 6.533; 6.667; 6.063; 6.267; 5.500; 5.875  // 4.0.1
#          6.643; 7.000; 6.333; 6.267; 7.385; 5.812; 6.063; 6.643; 6.500  // 3.0.8
MIN_CPU_RATIO_BIN_WCOMPR_ON_OFF = 4 if platform.system() == 'Linux' else 4

init_script = f"""
    create domain dm_dump varchar({DATA_WIDTH}) character set octets;
    create table t_common_text(s dm_dump);
    create table t_binary_data(s dm_dump);
    set term ^;
    execute block as
        declare i int = {N_ROWS};
    begin
        while (i > 0)do
        begin
            insert into t_common_text(s) values( lpad('',{DATA_WIDTH}, 'A') );
            insert into t_binary_data(s) values( lpad(_octets '',{DATA_WIDTH}, gen_uuid()) );
            i = i - 1;
        end
    end
    ^
    create or alter procedure sp_emitter(a_compressable boolean, n_limit int default 1)
    returns (b dm_dump) as
    begin
        while (n_limit > 0) do
        begin
            b = lpad('',{DATA_WIDTH}, iif(a_compressable, 'A', uuid_to_char(gen_uuid())));
            n_limit = n_limit - 1;
            suspend;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script, charset = 'none')

act = python_act('db')

@pytest.mark.perf_measure
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    # OPTIONAL CHECK: minimal ratio between CPU user time when WireCompression = false
    # and engine sends to client: 1) 'IDEALLY COMPRESSABLE'; 2) 'ABSOLUTELY IMCOMPRESSIBLE' data.
    # This can be useful to estimate record-level compression: textual data can be compressed
    # very well, so its transfer must take much less time then for GEN_UUID.
    # Experiments show that this ratio can be on Windows in the scope 0.3.... 0.8,
    # but on Linux it can be 1.0 or even slightly more then 1(!)
    # Windows: 0.500; 0.600; 0.571; 0.428; 0.714; 0.667; 0.625; 0.429; 0.571 // 4.0.1
    #          0.454; 0.357; 0.357; 0.500; 0.250; 0.385; 0.333; 0.200; 0.500 // 3.0.8
    # Linux:   0.684; 1.000; 1.000; 1.067; 0.867; 0.813; 0.800; 0.777; 0.750 // 4.0.1
    #          0.928; 1.000; 0.933; 1.000; 1.077; 0.750; 0.938; 0.929; 0.928 // 3.0.8
    if act.is_version('<4'):
        # 16.09.2022, 4.3.0.8 Classic: Windows -> 1.2
        MAX_CPU_RATIO_TXT2BIN_WCOMPR_OFF = 1.1 if platform.system() == 'Linux' else 1.5
    else:
        # 05.03.2023, 5.0.0.970, SS, Linux: 1.1 --> 1.3
        # 05.04.2023, 5.0.0.1001, SS, WIndows: 0.95 --> 1.5
        MAX_CPU_RATIO_TXT2BIN_WCOMPR_OFF = 1.3 if platform.system() == 'Linux' else 1.5

    # Register Firebird server (D:\FB\probes\fid-set-dpb-probe-05x.py)
    srv_cfg = driver_config.register_server(name = 'test_srv_core_0733', config = '')

    #srv_config_key_value_text = \
    #f"""
    #    [test_srv_core_0733]
    #    protocol = inet
    #"""
    #driver_config.register_server(name = 'test_srv_core_0733', config = srv_config_key_value_text)
    #db_cfg_object = driver_config.register_database(name = 'test_db_core_0733')
    #db_cfg_object.database.value = str(act.db.db_path)

    benchmark_data = defaultdict(list)

    for w_compr in (True, False):
        
        db_cfg_name = f'test_db_core_0733__wcompr_{w_compr}'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)
        db_cfg_object.server.value = srv_cfg.name
        # db_cfg_object.protocol.value = None /  XNET /  WNET -- these can not be used in this test!
        db_cfg_object.protocol.value = NetProtocol.INET
        db_cfg_object.database.value = str(act.db.db_path)
        db_cfg_object.config.value = f"""
            WireCrypt = Disabled
            WireCompression = {w_compr}
        """
        for i in range(0, N_MEASURES):
            with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
                prot_name = db_cfg_object.protocol.value.name if db_cfg_object.protocol.value else 'Embedded'
                assert w_compr == con.info.is_compressed(), f'Protocol: {prot_name} - can not be used to measure wire_compression'

                with con.cursor() as cur:
                    cur.execute('select mon$server_pid, mon$remote_protocol as p from mon$attachments where mon$attachment_id = current_connection')
                    for r in cur:
                        fb_pid, connection_protocol = r
                        assert connection_protocol.startswith('TCP'), f'Invalid connection protocol: {connection_protocol}'

                    for data_source in ('t_common_text', 't_binary_data'):
                        cur.execute('select s from ' + data_source)

                        fb_info_a = psutil.Process(fb_pid).cpu_times()
                        for r in cur:
                            # *** FULL FETCH ***
                            pass
                        fb_info_b = psutil.Process(fb_pid).cpu_times()

                        k = ('WireCompression=%d' % (1 if w_compr else 0), 'DataSource='+data_source)
                        benchmark_data[k] += fb_info_b.user - fb_info_a.user,


    compressed_txt = [ v for k,v in benchmark_data.items() if k[0] == 'WireCompression=1' and k[1] == 'DataSource=t_common_text'][0]
    compressed_bin = [ v for k,v in benchmark_data.items() if k[0] == 'WireCompression=1' and k[1] == 'DataSource=t_binary_data'][0]
    non_comprs_txt = [ v for k,v in benchmark_data.items() if k[0] == 'WireCompression=0' and k[1] == 'DataSource=t_common_text'][0]
    non_comprs_bin = [ v for k,v in benchmark_data.items() if k[0] == 'WireCompression=0' and k[1] == 'DataSource=t_binary_data'][0]

    cpu_txt_wcompr_ON_vs_OFF = median(compressed_txt) / max(0.00001, median(non_comprs_txt))
    cpu_bin_wcompr_ON_vs_OFF = median(compressed_bin) / max(0.00001, median(non_comprs_bin))
    cpu_txt2bin_wcompr_OFF = median(non_comprs_txt) / max(0.00001, median(non_comprs_bin))

    msg_result1 = 'CPU time ratio when sending %s and WireCompression is ON vs OFF: %s'
    what_sent1 = 'COMPRESSABLE TEXTUAL DATA'
    all_fine = 1
    if cpu_txt_wcompr_ON_vs_OFF > MIN_CPU_RATIO_TXT_WCOMPR_ON_OFF:
        print(msg_result1 % (what_sent1, 'EXPECTED') )
    else:
        print(msg_result1 % (what_sent1, '/* perf_issue_tag */ UNEXPECTED less than '+str(MIN_CPU_RATIO_TXT_WCOMPR_ON_OFF)) )
        all_fine = 0


    what_sent2 = 'INCOMPRESSABLE BINARY DATA'
    if cpu_bin_wcompr_ON_vs_OFF > MIN_CPU_RATIO_BIN_WCOMPR_ON_OFF:
        print(msg_result1 % (what_sent2, 'EXPECTED') )
    else:
        print(msg_result1 % (what_sent2, '/* perf_issue_tag */ UNEXPECTED, less than '+str(MIN_CPU_RATIO_BIN_WCOMPR_ON_OFF)) )
        all_fine = 0


    msg_result2 = 'CPU time ratio when WireCompression is OFF and sending TEXTUAL vs BINARY data: %s'
    if cpu_txt2bin_wcompr_OFF <= MAX_CPU_RATIO_TXT2BIN_WCOMPR_OFF:
        print(msg_result2 % ('EXPECTED') )
    else:
        print(msg_result2 % ('/* perf_issue_tag */ UNEXPECTED, more than ' + str(MAX_CPU_RATIO_TXT2BIN_WCOMPR_OFF)) )
        all_fine = 0

    if not all_fine:
        print('compressed_txt=',compressed_txt,' min=',min(compressed_txt),' max=',max(compressed_txt))
        print('compressed_bin=',compressed_bin,' min=',min(compressed_bin),' max=',max(compressed_bin))
        print('non_comprs_txt=',non_comprs_txt,' min=',min(non_comprs_txt),' max=',max(non_comprs_txt))
        print('non_comprs_bin=',non_comprs_bin,' min=',min(non_comprs_bin),' max=',max(non_comprs_bin))

        print('median(compressed_txt) / median(non_comprs_txt) =',1.000 * median(compressed_txt) / max(0.00001, median(non_comprs_txt)) )
        print('median(compressed_bin) / median(non_comprs_bin) =',1.000 * median(compressed_bin) / max(0.00001, median(non_comprs_bin)) )
        print('median(non_comprs_txt) / median(non_comprs_bin) =',1.000 * median(non_comprs_txt) / max(0.00001, median(non_comprs_bin)) )


    expected_stdout = ''
    for f in (what_sent1, what_sent2):
        expected_stdout += ''.join( (msg_result1 % (f, 'EXPECTED'), '\n')  )
    expected_stdout += (msg_result2 % 'EXPECTED')

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
