#coding:utf-8

"""
ID:          issue-7141
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7141
TITLE:       Services manager breaks long lines into 1023 bytes portions when using isc_info_svc_line in Service::query()
DESCRIPTION:
    Test invokes fbsvcmgr utility with requirement to start sepaarate trace session which will have long name.
    This name is stored in variable LONG_NAME_OF_TRACE_SESSION and its maximal len currently is 16254.
    Then we try to find this name in two ways:
        * using act.connect_server().trace.sessions.items();
        * using fbsvcmgr action_trace_list.
    Both way must return info which contains <LONG_NAME_OF_TRACE_SESSION> without line breaking (this is what was fixed).
NOTES:
    [03.02.2026] pzotov
    :::: ACHTUNG :::
    The source code of may differ depending on installation from git-repo vs pip install.
    There are special requirements to several parts of its source code related to this test:
        1) class TraceSession defined in $FB_DRV_HOME/types.py must contain 'plugins' field:
              plugins: list = field(default_factory=list)
              # 25.08.2025, discussed with pcisar, subj: "firebird-driver can't parse content of srv.trace.sessions since f9ac3d34"
           // otherwise test can raise TypeError: TraceSession.__init__() got an unexpected keyword argument 'plugins'
        2) class ServerTraceServices defined in $FB_DRV_HOME/core.py must contain branch related to output of trace plugins:
              ...
              elif line.lstrip().startswith('plugins:'):
                  current['plugins'] = line.split(':')[1].strip().split(',')
                  # 25.08.2025, discussed with pcisar, subj: "firebird-driver can't parse content of srv.trace.sessions since f9ac3d34"
    See also: https://github.com/FirebirdSQL/python3-driver/issues/55
    Test will raise RUNTIME EXCEPTION if firebird-driver source has no such additions.
    Trace log remains opened by fbsvcmgr in that case and subsequent runs of this test will fail on teardown phase with
    'Wind0wsError: [Err0r 32] The pr0cess cann0t... <trace_log>'
    // typos are intentional to avoid false positive pattern search if this test fails
    Because of that, it was decided to create trace log in the _OS_ temp dir with random name that matches pattern 'gh-7141.*.tmp'
    
    Confirmed bug on 4.0.1.2735, 3.0.10.33570: name of session did contain LF characters in its middle points.
    Confirmed problem on 5.0.0.418 - but only for console FB utility (fbsvcmgr), and NOT for usage firebird-QA framework
    (it causes BUGCHECK "decompression overran buffer (179), file: sqz.cpp line: 293" on test teardown phase).
    Checked on 6.0.0.363, 5.0.1.1408, 4.0.5.3103, 3.0.12.33744.
"""

import pytest
import platform
import re
from firebird.qa import *
from pathlib import Path
import subprocess
import time
import tempfile
import random
import string
from firebird.driver import InterfaceError

#db = db_factory(async_write = False)
db = db_factory()
act = python_act('db')

# cleanup temp dir: remove all files that could remain from THIS test previous runs:
# -----------------
trc_log_path = Path(tempfile.gettempdir())
trc_prefix = 'gh-7141'
trc_log_ptrn = f'{trc_prefix}.*.tmp'
for file_path in trc_log_path.glob(trc_log_ptrn):
    if file_path.is_file(): # Ensure it's a file and not a directory
        try:
            file_path.unlink(missing_ok = True)
        except OSError as e:
            pass

RANDOM_TRACE_LOG = trc_log_path / ( trc_prefix + '.' + ''.join(random.choices(string.ascii_lowercase + string.digits, k=8)) + '.tmp')
tmp_trace_cfg = temp_file(f'{trc_prefix}.trc.cfg')

MAX_WAIT_FOR_TRACE_STOP = 10
TRC_SESSION_NAME_PREFIX = 'gh_7141_'
TRC_SESSION_NAME_MAX_LEN = 16254

# 65000 100000 -->  FileNotFoundError: [WinError 206]  The filename or extension is too long // in localized form!
# 32000  --> AssertionError: Could not find trace session to be stopped in {act.connect_server().trace.sessions.items()=} // None
LONG_NAME_OF_TRACE_SESSION = (TRC_SESSION_NAME_PREFIX * 10000000)[:TRC_SESSION_NAME_MAX_LEN]
EXPECTED_MSG1 = 'Success: found trace session name in act.connect_server().trace.sessions.items()'
EXPECTED_MSG2 = 'Success: found trace session name in the result of fbsvcmgr action_trace_list'

@pytest.mark.trace
@pytest.mark.version('>=3.0.10')
def test_1(act: Action, tmp_trace_cfg: Path, capsys):

    trace_txt = f"""
        database=%[\\\\/]{act.db.db_path.name}
        {{
            enabled = true
            log_initfini = false
        }}
    """

    tmp_trace_cfg.write_text(trace_txt)
    trace_session_id = -1
    trace_session_nm = ''

    with RANDOM_TRACE_LOG.open('w') as f_log:
        # EXPLICIT call of FB utility 'fbsvcmgr':
        p = subprocess.Popen( [ act.vars['fbsvcmgr'],
                                'localhost:service_mgr',
                                'user', act.db.user,
                                'password', act.db.password,
                                'action_trace_start',
                                'trc_name', LONG_NAME_OF_TRACE_SESSION,
                                'trc_cfg', tmp_trace_cfg
                              ], 
                              stdout = f_log, stderr = subprocess.STDOUT
                            )
        time.sleep(1.1)

        q1 = subprocess.run( [ act.vars['fbsvcmgr'],
                              'localhost:service_mgr',
                              'user', act.db.user,
                              'password', act.db.password,
                              'action_trace_list',
                            ], 
                            stdout = f_log, stderr = subprocess.STDOUT
                         )

        assert q1.returncode == 0

        v_error_msg = ''
        with act.connect_server() as srv:
            # K = 1
            # V = TraceSession(id=1, user='SYSDBA', timestamp=..., name=<LONG_NAME_OF_TRACE_SESSION>, flags=['active', ' trace'])

            ### ACHTUNG ###
            # Source code of firebird-driver must meet following requirements:
            #     class TraceSession defined in $FB_DRV_HOME/types.py must contain 'plugins' field:
            #         plugins: list = field(default_factory=list)
            #         # 25.08.2025, discussed with pcisar, subj: "firebird-driver can't parse content of srv.trace.sessions since f9ac3d34"
            #     // otherwise test can raise TypeError: TraceSession.__init__() got an unexpected keyword argument 'plugins'
            #     class ServerTraceServices defined in $FB_DRV_HOME/core.py must contain branch related to output of trace plugins:
            #         ...
            #         elif line.lstrip().startswith('plugins:'):
            #             current['plugins'] = line.split(':')[1].strip().split(',')
            #             # 25.08.2025, discussed with pcisar, subj: "firebird-driver can't parse content of srv.trace.sessions since f9ac3d34"
            # See also: https://github.com/FirebirdSQL/python3-driver/issues/55
            # Test will raise RUNTIME EXCEPTION if firebird-driver source has no such additions.
            # Trace log remains opened by fbsvcmgr in that case and subsequent runs of this test will fail on teardown phase with
            # 'Wind0wsError: [Err0r 32] The pr0cess cann0t... <trace_log>'
            try:
                for k,v in srv.trace.sessions.items():
                    if v.flags[0] == 'active' and v.name.startswith(TRC_SESSION_NAME_PREFIX):
                        trace_session_id = v.id
                        trace_session_nm = v.name
            except (TypeError, InterfaceError) as e:
                v_error_msg = e.__str__()
            
        assert v_error_msg == '', 'Source code of firebird-driver probably needs to be updated.'
        assert trace_session_id > 0, f'Could not find trace session to be stopped in {act.connect_server().trace.sessions.items()=}'

        q2 = subprocess.run( [ act.vars['fbsvcmgr'],
                              'localhost:service_mgr',
                              'user', act.db.user,
                              'password', act.db.password,
                              'action_trace_stop',
                              'trc_id', str(trace_session_id)
                            ], 
                            stdout = f_log, stderr = subprocess.STDOUT,
                            timeout = MAX_WAIT_FOR_TRACE_STOP
                         )

        time.sleep(1.1)
        if not p.poll():
            p.terminate()
        assert q2.returncode == 0
    
    if trace_session_nm == LONG_NAME_OF_TRACE_SESSION:
        print(EXPECTED_MSG1)
    else:
        print('UNEXPECTED. COULD NOT FIND trace session name in in act.connect_server().trace.sessions.items()')

    p_prefix_in_list = re.compile(f'name(:)?\\s+{TRC_SESSION_NAME_PREFIX}', re.IGNORECASE)

    found_in_trc_list = False
    with RANDOM_TRACE_LOG.open('r') as f_log:
        for line in f_log:
            #if p_prefix_in_list.search(line):
            if LONG_NAME_OF_TRACE_SESSION in line:
                found_in_trc_list = True
                print(EXPECTED_MSG2)
                break

    if not found_in_trc_list:
        print('Check result of fbsvcmgr action_trace_list:')
        with RANDOM_TRACE_LOG.open('r') as f:
            trace_lines = [ x for x in f.read().splitlines() if x.split() ]
            for i, x in enumerate(trace_lines):
                print(f'line {i}, length = {len(x.rstrip())}: >' + x.rstrip() + '<')

    act.expected_stdout = f"""
        {EXPECTED_MSG1}
        {EXPECTED_MSG2}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
