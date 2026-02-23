#coding:utf-8

"""
ID:          issue-333be4bf
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/333be4bfd48d8b2f84852073436a20698dbc8c54
TITLE:       Change error by warning in the case when isc_dpb_parallel_workers value is not at valid range
DESCRIPTION:
    Test creates backup and tries to restore it using gbak utility (with '-v' switch).
    Three iterations is performed with restore:
        * via local (embedded) protocol;
        * via remote protocol without using Services API;
        * via remote protocol WITH usage Services API (i.e. with '-se' command switch);
    Actual value of MaxParallelWorkers is evaluated by querying to RDB$CONFIG table.
    We add 1 to this value and pass it to gbak '-par' option. Output of gbak is redirected to log.
    This log must contain 'gbak: WARNING:' message and must NOT have 'gbak: ERROR:' ones.
NOTES:
    [18.11.2023] pzotov
    Log for 5.0.0.995 (01-apr-2023) contain ERRORS:
        gbak: ERROR:bad parameters on attach or create database
        gbak: ERROR:    Wrong parallel workers value 9, valid range are from 1 to 8
        gbak: ERROR:failed to create database localhost:<tmp_fdb>
        gbak:Exiting before completion due to errors
    Log for 5.0.0.1007 (05-apr-2023) has only WARNING:
        gbak: WARNING:Wrong parallel workers value 9, valid range are from 1 to 8
        ...
        gbak:adjusting the ONLINE and FORCED WRITES flags

    Checked on 5.0.0.1271 (CS/SS), 6.0.0.132 (CS/SS)
"""
from pathlib import Path
import subprocess

import pytest
from firebird.qa import *

tmp_fbk = temp_file('tmp_333be4bf.fbk')
tmp_res = temp_file('tmp_333be4bf.fdb')
tmp_log = temp_file('tmp_333be4bf.log')

db = db_factory()

substitutions = [ (  '^((?!(iter:|(gbak:[ \t]?(ERROR:|WARNING:)))).)*$' , ''  ) ]

act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=5.0')
def test_1(act: Action, tmp_fbk: Path, tmp_res: Path, tmp_log: Path, capsys):

    max_parallel_wrk = -1
    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute("select g.rdb$config_value from rdb$config g where upper(g.rdb$config_name) = upper('MaxParallelWorkers')")
        for r in cur:
            max_parallel_wrk = int(r[0])
    
    assert max_parallel_wrk > 1, "Config parameter 'MaxParallelWorkers' must have value greater than 1"

    act.gbak(switches=['-b', str(act.db.dsn), str(tmp_fbk)])

    for iter in ('embedded', 'no_use_svc','using_svc'):
        with open(tmp_log,'w') as f:
            if iter == 'embedded':
                subprocess.call( [
                                    act.vars['gbak']
                                   ,'-rep'
                                   ,'-v'
                                   ,'-par', str(max_parallel_wrk+1)
                                   ,tmp_fbk
                                   ,tmp_res
                                 ]
                                 ,stdout = f, stderr = subprocess.STDOUT
                               )
            elif iter == 'no_use_svc':
                subprocess.call( [
                                    act.vars['gbak']
                                   ,'-rep'
                                   ,'-user', act.db.user
                                   ,'-pass', act.db.password
                                   ,'-v'
                                   ,'-par', str(max_parallel_wrk+1)
                                   ,tmp_fbk
                                   ,'localhost:' + str(tmp_res)
                                 ]
                                 ,stdout = f, stderr = subprocess.STDOUT
                               )
            else:
                subprocess.call( [
                                    act.vars['gbak']
                                   ,'-rep'
                                   ,'-user', act.db.user
                                   ,'-pass', act.db.password
                                   ,'-v'
                                   ,'-par', str(max_parallel_wrk+1)
                                   ,'-se', 'localhost:service_mgr'
                                   ,tmp_fbk
                                   ,tmp_res
                                 ]
                                 ,stdout = f, stderr = subprocess.STDOUT
                               )
        print(f'iter: {iter}')
        with open(tmp_log,'r') as f:
            for line in f:
                print(line)

        act.expected_stdout = f"""
            iter: {iter}
            gbak: WARNING:Wrong parallel workers value {max_parallel_wrk+1}, valid range are from 1 to {max_parallel_wrk}
        """

        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

