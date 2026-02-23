#coding:utf-8

"""
ID:          issue-6709
ISSUE:       6709
TITLE:       gbak discards replica mode
DESCRIPTION:
    https://github.com/FirebirdSQL/firebird/issues/6709
    Confirmed bug on 4.0.0.2353: 'replica' flag was not preserved after restoring DB.
    Checked on: 4.0.1.2624, 5.0.0.244 -- all OK.
JIRA:        CORE-6478
FBTEST:      bugs.gh_6709
NOTES:
    [29.06.2022] pzotov
    Checked on 4.0.1.2692, 5.0.0.509. Re-check reproducing of problem on 4.0.0.2353.
"""

import locale
import pytest
from firebird.qa import *
from pathlib import Path
from firebird.driver import ReplicaMode

db = db_factory()

tmp_fbk = temp_file( filename = 'tmp_gh_6709.fbk')
tmp_res = db_factory(filename='tmp_gh_6709.tmp')

act = python_act('db', substitutions=[('[ \t]+', ' ')])

chk_sql = """
    set heading off;
    select rdb$get_context('SYSTEM', 'REPLICA_MODE') as "gfix -repl %(r_mode)s"
    from rdb$database r
    ;
"""

@pytest.mark.version('>=4.0')

def test_1(act: Action, tmp_fbk: Path, tmp_res: Database, capsys):

    for r_mode in ('read_only', 'read_write', 'none'):
        # -----------------------------------------------------
        act.gfix(switches=['-replica', r_mode, act.db.dsn], io_enc = locale.getpreferredencoding())
        act.expected_stdout = r_mode.upper().replace('_','-') if r_mode != 'none' else '<null>'
        act.isql(switches=['-q'], input = chk_sql % locals())
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
        # -----------------------------------------------------
        act.gbak(switches=['-b', str(act.db.db_path), str(tmp_fbk)], io_enc = locale.getpreferredencoding())
        act.gbak(switches=['-rep', str(tmp_fbk), tmp_res.db_path], io_enc = locale.getpreferredencoding())
        #act.gbak(switches=['-b', '-se', 'localhost:service_mgr', str(act.db.db_path), str(tmp_fbk)])
        #act.gbak(switches=['-rep', '-se', 'localhost:service_mgr', str(tmp_fbk), str(tmp_res)])

        act.expected_stdout = r_mode.upper().replace('_','-') if r_mode != 'none' else '<null>'
        act.isql(switches=['-q'], input = chk_sql % locals(), use_db = tmp_res, io_enc = locale.getpreferredencoding())
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
