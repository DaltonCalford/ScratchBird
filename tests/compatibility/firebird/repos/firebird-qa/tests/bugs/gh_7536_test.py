#coding:utf-8

"""
ID:          issue-7537
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7537
TITLE:       Add ability to query current value of parallel workers for an attachment
NOTES:
    [03.04.2023] pzotov
    ::: NB :::
    This is INITIAL version of test.
    It checks only *ability* to query values from mon$attachments and rdb$get_context.
    It does NOT check tag fb_info_parallel_workers because this is not yet implemented
    (BTW, 'isc_dpb_parallel_workers' also not yet avaliable in the firebird-driver).
    ==============
    doc/README.parallel_features.txt:
    Default value is 1 and means no use of additional parallel workers. Value in
    DPB have higher priority than setting in firebird.conf.
    ...
    ParallelWorkers - set default number of parallel workers that used by user attachments. 
    Could be overriden by attachment using tag isc_dpb_parallel_workers in DPB.
    ==============

    Checked on intermediate snapshot 5.0.0.1001.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 0 * mon$parallel_workers as chk_mon_parallel from mon$attachments where mon$attachment_id = current_connection;
    select 0 * cast(rdb$get_context('SYSTEM', 'PARALLEL_WORKERS') as int) as chk_ctx_parallel from rdb$database;
"""

expected_stdout = """
    CHK_MON_PARALLEL                0
    CHK_CTX_PARALLEL                0
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
