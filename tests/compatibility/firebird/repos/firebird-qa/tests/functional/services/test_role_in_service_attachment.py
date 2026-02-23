#coding:utf-8
"""
ID:          services.role-in-service-attachment
TITLE:       Check that trace plugin shows ROLE used in service attachment. Format: USER[:ROLE]
DESCRIPTION:
  See: https://github.com/FirebirdSQL/firebird/commit/dd241208f203e54a9c5e9b8b24c0ef24a4298713
FBTEST:      functional.services.role_in_service_attachment

NOTES:
    [30.07.2022] pzotov
    Current version of QA plugin *does* allow to specify user/password/role when we connect to Services API and launch trace.
    But actually this data is not used in by plugin when it launched trace, see 'def trace_thread'.
    It seems that 'with act.connect_server(...)' invocation must use these parameters also, otherwise trace log shows that
    it was started by SYSDBA.
    Sent report to pcisar, 30.07.2022 18:33. Waiting for confirmation or other solution.

    [20.09.2022] pzotov
    Replaced code which invokes trace with EXPLICIT call of fbsvcmgr utility via subprocess.Popen. Otherwise always got:
    fail with: "ERROR: trace output contains only USER, without ROLE."
    See also: bugs/core_3658_test.py

    Checked on 4.0.1.2692 (SS/CS), 5.0.0.730 (SS/CS)
"""

import subprocess
import time
import re
import pytest
from pathlib import Path

from firebird.qa import *
import firebird.driver
from firebird.driver import SrvStatFlag

tmp_user = user_factory('db', name='tmp_tracing_user', password='123')
db = db_factory()
tmp_role = role_factory('db', name='tmp_tracing_role')

tmp_trace_cfg = temp_file('tmp_trace_role_in_svc_attachment.cfg')
tmp_trace_log = temp_file('tmp_trace_role_in_svc_attachment.log')

act = python_act('db')

#expected_stdout = 'SUCCESS: found expected line format in the trace log: <USER>:<ROLE>'

@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User, tmp_role:Role, tmp_trace_cfg: Path, tmp_trace_log: Path, capsys):

    init_script = \
    f'''
        set wng off;
        set bail on;
        revoke all on all from {tmp_user.name};
        commit;
        -- Trace other users' attachments
        alter role {tmp_role.name} set system privileges to TRACE_ANY_ATTACHMENT;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;
    '''
    act.isql(switches=['-q'], input=init_script)


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

    with tmp_trace_log.open('w') as f_log:
        # EXPLICIT call of FB utility 'fbsvcmgr':
        p = subprocess.Popen( [ act.vars['fbsvcmgr'],
                                'localhost:service_mgr',
                                'user', tmp_user.name,
                                'password', tmp_user.password,
                                'role', tmp_role.name,
                                'action_trace_start', 'trc_cfg', tmp_trace_cfg
                              ], 
                              stdout = f_log, stderr = subprocess.STDOUT
                            )
        time.sleep(2)

        # ::: DO NOT USE HERE :::
        # with act.trace(svc_events = svc_items, ...):
        #    pass

        p.terminate()


    # Windows: service_mgr, (Service 00000000105E93C0, TMP_TRACING_USER:TMP_TRACING_ROLE, TCPv6:::1/53682, D:\FB\fb401rls\fbsvcmgr.exe:18032)
    # Linux:   service_mgr, (Service 0x7fc58f6073c0, TMP_TRACING_USER:TMP_TRACING_ROLE, TCPv6:::1/35666, /opt/fb40/bin/fbsvcmgr:218534)
    p = re.compile('service_mgr,\\s+\\(\\s*Service\\s+\\w+[,]?\\s+' + tmp_user.name + ':' + tmp_role.name + '[,]?', re.IGNORECASE)
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
