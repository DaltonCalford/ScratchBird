#coding:utf-8
"""
ID:          issue-7139
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7139
TITLE:       With multiple trace sessions user may receive trace events related to engine's requests
DESCRIPTION: 
    Test launces TWO trace sessions: one using FB utility 'fbsvcmgr' and second using firebird-qa ("with act.trace: ...").
    ID of trace session that was launched via fbsvcmgr is stored in order to have ability to stop it (again using fbsvcmgr).
    Trace session #2 (that is launched using firebird-qa) is performed in context manager, and we run ISQL there with trivial requirement
    just to make QUIT after connection establishing.
    Trace #2 must NOT issue any messages after that (with exception of 'SET TRANSACTION' which is suppressed by appropriate exclude_filer).
    ::: NB :::
    Timeout about 1.1 second is required after each launching of FB utility fbsvcmgr otherwise either trace session ID can remain None or
    we can encounter with 'WindowsError: [Error 32] The process cannot access the file because it is being used by another process' on
    attempt to delete trace log after 'with' block ends (fbsvcmgr made requests to Services API with interval =  1.0 second).
NOTES:
    [26.02.2023] pzotov
    1. Initially discussed by email, started 28-feb-2022, subj:
        Firebird new-QA: weird result for trivial test (outcome depends on presence of... running trace session!)
    2. One of trace sessions must be launched using fbsvcmgr, NOT using "with act.trace ..." (otherwise issue will not be reproduced).

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
    'WindowsError: [Error 32] The process cannot access the file... <trace_log>'
    Because of that, it was decided to create trace log in the _OS_ temp dir with random name that matches pattern 'gh-7139.*.tmp'

    Confirmed problem on: 5.0.0.514, 4.0.1.2735, got in trace (as reaction on connect + quit):
        select RDB$RELATION_NAME from RDB$USER_PRIVILEGES where RDB$USER = ? and RDB$PRIVILEGE = 'M' and RDB$USER_TYPE = 8 and RDB$OBJECT_TYPE = 13
    Checked on 5.0.0.518 SS/CS; 4.0.1.2736 SS/CS -- all fine: no any message will be issued when two trace sessions exist and we do connect plus quit.
"""

import pytest
import platform
from firebird.qa import *
from pathlib import Path
import subprocess
import time
import tempfile
import random
import string
from firebird.driver import InterfaceError


# cleanup temp dir: remove all files that could remain from THIS test previous runs:
# -----------------
trc_log_path = Path(tempfile.gettempdir())
trc_prefix = 'gh-7139'
trc_log_ptrn = f'{trc_prefix}.*.tmp'
for file_path in trc_log_path.glob(trc_log_ptrn):
    if file_path.is_file(): # Ensure it's a file and not a directory
        try:
            file_path.unlink(missing_ok = True)
        except OSError as e:
            pass

RANDOM_TRACE_LOG = trc_log_path / ( trc_prefix + '.' + ''.join(random.choices(string.ascii_lowercase + string.digits, k=8)) + '.tmp')
tmp_trace_cfg = temp_file(f'{trc_prefix}.trc.cfg')

# ::: NB ::: We have to use DB in OS temp dir rather than in folder defined as Pytest --basetemp!
# Otherwise loop of this test will fail at 2nd iter with 'object in use' because database remains opened for ~20 seconds
# after 'killer' connection completes. Attempt to close DB faster using 'gfix -shut...' DOES NOT HEELP!
#
RANDOM_TRACE_LOG = trc_log_path / ( trc_prefix + '.' + ''.join(random.choices(string.ascii_lowercase + string.digits, k=8)) + '.tmp')
tmp_trace_cfg = temp_file(f'trace.{trc_prefix}.cfg')
 
db = db_factory()
act = python_act('db')
expected_stdout = """
"""
 
test_script = """
    quit;
"""
 
#-------------------------------------------------------------------------

def get_external_trace_id(act: Action, a_what_to_check, a_ext_trace_session_name):
    v_fbsvcmgr_trace_sess_id = -1
    v_error_msg = ''
    with act.connect_server() as srv:
        # 1 ::: TraceSession(id=1, user='SYSDBA', timestamp=datetime.datetime(2023, 2, 26, 14, 52, 24), name='trc_gh_7139', flags=['active', ' trace'])
        # 2 ::: TraceSession(id=2, user='SYSDBA', timestamp=datetime.datetime(2023, 2, 26, 14, 53, 58), name='', flags=['active', ' trace'])

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
        # 'WindowsError: [Error 32] The process cannot access the file... <trace_log>'
        try:
            for k,v in srv.trace.sessions.items():
                if v.name == a_ext_trace_session_name:
                    v_fbsvcmgr_trace_sess_id = v.id
        except (TypeError, InterfaceError) as e:
            v_error_msg = e.__str__()

    assert v_error_msg == '', 'Source code of firebird-driver probably needs to be updated.'

    msg_prefix = 'Required trace session (launched by fbsvcmgr utility)'
    if a_what_to_check == 'running':
        assert v_fbsvcmgr_trace_sess_id > 0,  msg_prefix + ' NOT found.'
    elif a_what_to_check == 'stopped':
        assert v_fbsvcmgr_trace_sess_id < 0,  msg_prefix + ' WAS NOT DROPPED.'

    return v_fbsvcmgr_trace_sess_id

#-------------------------------------------------------------------------

@pytest.mark.trace
@pytest.mark.version('>=4.0.2')
def test_1(act: Action, tmp_trace_cfg: Path, capsys):

    # C:\temp\pytest-of-PashaZ\pytest-513\test_10\TEST.FDB
    #    database=[\\\\/]{act.db.db_path}
    trace_txt = f"""
        database=%[\\\\/]{act.db.db_path.name}
        {{
            enabled = true
            log_initfini = false
            log_errors = true
            log_statement_finish = true
            time_threshold = 0
        }}
    """

    tmp_trace_cfg.write_text(trace_txt)

    EXTERNAL_TRC_SESSION_NAME = 'trc_gh_7139'
    MAX_WAIT_FOR_TRACE_STOP = 30

    with RANDOM_TRACE_LOG.open('w') as f_log:
        # EXPLICIT call of FB utility 'fbsvcmgr':
        p = subprocess.Popen( [ act.vars['fbsvcmgr'],
                                'localhost:service_mgr',
                                'user', act.db.user,
                                'password', act.db.password,
                                'action_trace_start',
                                'trc_name', EXTERNAL_TRC_SESSION_NAME,
                                'trc_cfg', tmp_trace_cfg
                              ], 
                              stdout = f_log, stderr = subprocess.STDOUT
                            )
        time.sleep(1.1)

        
        trc_items_list = [
                 'log_initfini = false',
                 'log_errors = true',
                 'log_statement_finish = true',
                 'time_threshold = 0',
                 'exclude_filter="(set transaction)"',
                 ]

        
        with act.trace(db_events=trc_items_list):
            act.isql(switches=['-q'], input=test_script, combine_output = True)

        # ID os trace session that we have started usign FB utility fbsvcmgr:
        v_fbsvcmgr_trace_sess_id = get_external_trace_id(act, 'running', EXTERNAL_TRC_SESSION_NAME)

        q = subprocess.run( [ act.vars['fbsvcmgr'],
                                   'localhost:service_mgr',
                                   'user', act.db.user,
                                   'password', act.db.password,
                                   'action_trace_stop',
                                   'trc_id', str(v_fbsvcmgr_trace_sess_id)
                                   ], 
                                   stdout = f_log,
                                   stderr = subprocess.STDOUT,
                                   timeout = MAX_WAIT_FOR_TRACE_STOP
                                 )

        if q.returncode:
            p.terminate()
        assert q.returncode == 0
        
        time.sleep(1.1)

        v_fbsvcmgr_trace_sess_id = get_external_trace_id(act, 'stopped', EXTERNAL_TRC_SESSION_NAME)

        act.expected_stdout = expected_stdout
        act.trace_to_stdout()
        assert act.clean_stdout == act.clean_expected_stdout

