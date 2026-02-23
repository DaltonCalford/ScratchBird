#coding:utf-8

"""
ID:          issue-7188
ISSUE:       7188
TITLE:       Memory leak in GDS32.DLL(FBClient.DLL), when multi-database transaction has done
DESCRIPTION:
    Test obtains PID of currently running Python script which serves as CLIENT for server.
    Then we get values of RSS and VMS by invoking client_process.memory_info() and store them.
    After that, we use DistributedTransactionManager instance for making two connections to different
    databases and run <LOOP_COUNT> times Tx start and commit (in loop).
    Finally, we get again RSS and VMS values for currently running Python process, and compare them
    with initially stored ones.
    Ratio between appropriate final and initial values of RSS and VMS must not exceed thresholds
    defined by RSS_MAX_DIFF and VMS_MAX_DIFF.
NOTES:
    [21.07.2024] pzotov
    Confirmed memory leak in the CLIENT process (i.e. python.exe) when running test on:
    * 5.0.0.511 (09-jun-2022): 
        memo_rss_list=[46988, 54192] diff = 1.15332
        memo_vms_list=[37048, 44228] diff = 1.19380
    * 4.0.2.2776 (10.06.2022):
        memo_rss_list=[47248, 54456] diff = 1.15255
        memo_vms_list=[37240, 44424] diff = 1.19291
    Aftef fix:
    * 5.0.1.514:
        memo_rss_list=[47164, 47184] diff = 1.000424
        memo_vms_list=[37200, 37200] no_diff
    * 4.0.2.2779:
        memo_rss_list=[47256, 47272] diff = 1.0003386
        memo_vms_list=[37172, 37172] no_diff
   Checked on 6.0.0.396, 5.0.1.1440, 4.0.53127.
   Thanks to Vlad for suggestions about test implementation.
"""
import os
import pytest
from firebird.qa import *
from firebird.driver import DistributedTransactionManager, tpb, Isolation
import psutil

db1 = db_factory(filename='core_7188_a.fdb')
db2 = db_factory(filename='core_7188_b.fdb')

tmp_user1 = user_factory('db1', name='tmp$7188_1', password='123')
tmp_user2 = user_factory('db2', name='tmp$7188_2', password='456')

act1 = python_act('db1')
act2 = python_act('db2')

LOOP_COUNT = 10000
CUSTOM_TPB = tpb(isolation = Isolation.READ_COMMITTED)
RSS_MAX_DIFF = 1.003
VMS_MAX_DIFF = 1.001
PASSED_MSG = 'OK'

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.2')
def test_1(act1: Action, act2: Action, tmp_user1: User, tmp_user2: User, capsys):
    dt_list = []
    memo_rss_list = []
    memo_vms_list = []
    client_process = psutil.Process( os.getpid() )
   
    memo_rss_list.append(int(client_process.memory_info().rss / 1024))
    memo_vms_list.append(int(client_process.memory_info().vms / 1024))
    with act1.db.connect(user = tmp_user1.name, password = tmp_user1.password) as con1, \
         act2.db.connect(user = tmp_user2.name, password = tmp_user2.password) as con2:
        
        for i in range(LOOP_COUNT):
            dt = DistributedTransactionManager([con1, con2])
            dt.begin(tpb = CUSTOM_TPB)
            dt.commit()

    memo_rss_list.append(int(client_process.memory_info().rss / 1024))
    memo_vms_list.append(int(client_process.memory_info().vms / 1024))

    if memo_rss_list[1] / memo_rss_list[0] < RSS_MAX_DIFF and memo_vms_list[1] / memo_vms_list[0] < VMS_MAX_DIFF:
        print(PASSED_MSG)
    else:
       print('client_process.memory_info(): ratio of RSS and/or VMS values exceeds threshold.')
       print(f'{memo_rss_list=}, ratio: {memo_rss_list[1] / memo_rss_list[0]:.3f}, {RSS_MAX_DIFF=:.3f}')
       print(f'{memo_vms_list=}, ratio: {memo_vms_list[1] / memo_vms_list[0]:.3f}, {VMS_MAX_DIFF=:.3f}')

    act1.expected_stdout = PASSED_MSG
    act1.stdout = capsys.readouterr().out
    assert act1.clean_stdout == act1.clean_expected_stdout
