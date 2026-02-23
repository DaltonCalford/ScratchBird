#coding:utf-8

"""
ID:          issue-6506
ISSUE:       6506
TITLE:       gbak with PIPE to stdout: invalid content if user '-se <host>:service_mgr' command switch
DESCRIPTION:
    Confirmed bug on 3.0.6.33276, 4.0.0.1850.
    Works fine on 3.0.6.33277, 4.0.0.1854
JIRA:        CORE-6264
FBTEST:      bugs.core_6264
NOTES:
    [29.08.2022] pzotov
    1. Re-checked on 3.0.6.33276, 4.0.0.1850 - problem confirmed.
    2. Checked on 5.0.0.691, 4.0.1.2692, 3.0.8.33535 - both Windows and Linux.
    3. In case when '-user ... -pas ...' is missed in p_sender command, error message looks weird
       (and the same as it was originally detected and shown in this ticket):
        gbak: ERROR:expected backup description record
        gbak:Exiting before completion due to errors
"""
import subprocess
from subprocess import PIPE
from pathlib import Path
import time

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

tmp_rest_fdb = db_factory(filename = 'core_4462_res.fdb', do_not_create = True, do_not_drop = True)
act_rest_fdb = python_act('tmp_rest_fdb')

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, act_rest_fdb: Action, capsys):

    p_sender = subprocess.Popen( [ act.vars['gbak'], '-user', 'SYSDBA', '-pas', 'masterkey', '-b', '-se', 'localhost:service_mgr', act.db.db_path, 'stdout' ], stdout=PIPE)
    p_getter = subprocess.Popen( [ act.vars['gbak'], '-rep', 'stdin',  act_rest_fdb.db.db_path ], stdin = p_sender.stdout, stdout = PIPE, stderr = subprocess.STDOUT)
    p_sender.stdout.close()
    # https://docs.python.org/2/library/subprocess.html#replacing-shell-pipeline
    p_getter_stdout, p_getter_stderr = p_getter.communicate()
    act_rest_fdb.db.db_path.unlink(missing_ok = True)

    print(p_getter_stdout.decode("utf-8"))

    act.expected_stdout = ''
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
