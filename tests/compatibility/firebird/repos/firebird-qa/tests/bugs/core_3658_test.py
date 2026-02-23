#coding:utf-8

"""
ID:          issue-4008
ISSUE:       4008
TITLE:       FBSVCMGR connects to server as OS user name rather than value of ISC_USER environment variable
DESCRIPTION:
JIRA:        CORE-3658
FBTEST:      bugs.core_3658
NOTES:
    [17.11.2021] pcisar
        Implementation is complicated, and IMHO not worth of realization
    [19.09.2022] pzotov
        We have to EXPLICITLY invoke fbsvcmgr in this test rather than call it using 'with act.trace()'.
        This is because act.trace() *always* will define user/password pair and substitute them into 
        returned TraceSession( ... ) instance, so we have no ability to call trace manager with IMPLICIT
        credentials, i.e. via ISC_* env. variables.

        See also: tests/functional/services/test_role_in_service_attachment.py 

        Checked on 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730 (SS/CS) -- Windows and Linux
"""
import os
import subprocess
import locale
import re
from pathlib import Path
import time

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

tmp_trace_cfg = temp_file('tmp_trace_3658.cfg')
tmp_trace_log = temp_file('tmp_trace_3658.log')

svc_items = [
                'log_services = true',
                'log_errors = true',
            ]

@pytest.mark.trace
@pytest.mark.version('>=3')
def test_1(act: Action, tmp_trace_cfg: Path, tmp_trace_log: Path, capsys):
    #print( os.environ.get('ISC_USER', 'UNKNOWN_ISC_USR') )
    #print( os.environ.get('ISC_PASSWORD', 'UNKNOWN_ISC_PSW') )

    trace_txt = """
        services
        {
            enabled = true
            log_initfini = false
            log_services = true
            log_errors = true
        }
    """

    tmp_trace_cfg.write_text(trace_txt)

    with act.envar('ISC_USER', act.db.user), act.envar('ISC_PASSWORD', act.db.password):

        with tmp_trace_log.open('w') as f_log:
            # EXPLICIT call of FB utility 'fbsvcmgr':
            p = subprocess.Popen( [act.vars['fbsvcmgr'], 'localhost:service_mgr', 'action_trace_start', 'trc_cfg', tmp_trace_cfg], stdout = f_log, stderr = subprocess.STDOUT )
            time.sleep(2)

            # ::: DO NOT USE HERE :::
            # with act.trace(svc_events = svc_items, ...):
            #    pass

            p.terminate()

    # Windows: service_mgr, (Service 0000000000C8B140, SYSDBA, TCPv6:::1/60775, ...)
    # Linux:   service_mgr, (Service 0x7fc58f6073c0, SYSDBA, TCPv6:::1/35666, ...)
    p = re.compile('service_mgr,\\s+\\(\\s*Service\\s+\\w+[,]?\\s+' + act.db.user+ '[,]?', re.IGNORECASE)

    expected_stdout = 'Found expected line: 1'
    with open(tmp_trace_log,'r') as f:
        for line in f:
            if line.strip():
                if p.search(line):
                    print(expected_stdout)
                    break

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
