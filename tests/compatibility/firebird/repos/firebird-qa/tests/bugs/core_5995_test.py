#coding:utf-8

"""
ID:          issue-6245
ISSUE:       6245
TITLE:       Creator user name is empty in user trace sessions
DESCRIPTION:
    We create trivial config for trace and start two trace sessions (see notes below).
    The log of 'fbsvcmgr action_trace_list' must contain two occurrences of 'user: <name>'.
JIRA:        CORE-5995
NOTES:
    [22.08.2025] pzotov
    Re-implemented:
    1) additional user is created with grant him system privilege to trace any attachment;
    2) on FB 6.x since 20-aug-2025 list of sessions contain name of plugin, see:
       https://github.com/FirebirdSQL/firebird/commit/f9ac3d34117ee7006be9cc0baca79b3aaf075111
       ("Print trace plugins in tracecmgr LIST output")
       Current version of firebird-driver can not correctly to handle this and fails on obtaining
       content of srv.trace.sessions with:
       "firebird.driver.types.InterfaceError: Unexpected line in trace session list: plugins: <default>"

       Because of that, it was decided to invoke fbsvcmgr utility as child process and parse its ouput.
    3) we search in the log file (that is result of 'fbsvcmgr action_trace_list') not only name of users
       who started trace sessions but also lines with session ID, flags and (for 6.x) name of plugin.
       Number of occurences of each item in the log must be equal to the number of users who started trace.
    4) Raised min_version to 4.0 because system privileges absent in 3.x.

    Test duration: ~10s.
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231.
"""

import subprocess
import time
import locale
import re
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory()

################
### SETTINGS ###
################
# max time we wait before launch fbsvcmgr to get trace sessions list, seconds:
MAX_WAIT_FOR_TRACE_SESSIONS_LOADING = 1

# max allowed time to obtain trace sessions list, seconds:
MAX_WAIT_TO_GET_TRACE_SESSIONS_LIST = 30
#...............

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

trace_options = \
    [ 'log_initfini = false',
      'time_threshold = 0',
      'log_statement_finish = true',
    ]

tmp_user = user_factory('db', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db', name='tmp_role_trace_any_attachment')

trc_lst = temp_file('tmp_5995_trace_sessions.txt')

@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User, tmp_role: Role, trc_lst: Path, capsys):

    init_script = f"""
        set wng off;
        set bail on;
        alter user {tmp_user.name} revoke admin role;
        revoke all on all from {tmp_user.name};
        commit;
        -- Trace other users' attachments
        alter role {tmp_role.name}
            set system privileges to TRACE_ANY_ATTACHMENT;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;
    """
    act.isql(switches=['-q'], input=init_script, combine_output = True)
    assert act.clean_stdout == '', f'Init script FAILED.'
    act.reset()

    with act.trace(db_events = trace_options, encoding=locale.getpreferredencoding()) as t1, \
         act.trace(db_events = trace_options, encoding=locale.getpreferredencoding(), user = tmp_user.name, password = tmp_user.password, role = tmp_role.name) as t2:

        time.sleep(MAX_WAIT_FOR_TRACE_SESSIONS_LOADING) # let services API to load traces

        with open(trc_lst, 'w') as f:
            p = subprocess.Popen([act.vars["fbsvcmgr"], "localhost:service_mgr", "user", act.db.user, "password", act.db.password, "action_trace_list"], stdout = f, stderr=subprocess.STDOUT)
            p.wait(MAX_WAIT_TO_GET_TRACE_SESSIONS_LIST)

    # Example:
    # Session ID: 5 
    #    user:    SYSDBA 
    #   date:    2025-08-22 15:49:47 
    #   flags:   active, trace 
    #   plugins: <default>  ----------- appeared since 6.0.0.1244

    p_tsid = re.compile('Session ID(:)?\\s+\\d+', re.IGNORECASE)
    p_user = re.compile(f'user(:)?\\s+({act.db.user}|{tmp_user.name})', re.IGNORECASE)
    p_flag = re.compile('flags(:)?\\s+active', re.IGNORECASE)
    p_plug = re.compile('plugins(:)?\\s+\\S+', re.IGNORECASE)

    with open(trc_lst, 'r') as f:
        trc_sessions_lst = f.readlines()
        trc_session_ids_cnt = len( [x for x in trc_sessions_lst if p_tsid.search(x)] )
        trc_user_names_cnt = len( [x for x in trc_sessions_lst if p_user.search(x)] )
        trc_flag_lines_cnt = len( [x for x in trc_sessions_lst if p_flag.search(x)] )
        trc_plugin_names_cnt = len( [x for x in trc_sessions_lst if p_plug.search(x)] )

    result_map = {'trc_session_ids_cnt' : trc_session_ids_cnt, 'trc_user_names_cnt' : trc_user_names_cnt, 'trc_flag_lines_cnt' : trc_flag_lines_cnt, 'trc_plugin_names_cnt' : trc_plugin_names_cnt}
    for k,v in result_map.items():
        print(k,':',v)


    trc_plugin_names_cnt = 0 if act.is_version('<6') else 2
    act.expected_stdout =  f"""
        trc_session_ids_cnt  :  2
        trc_user_names_cnt   :  2
        trc_flag_lines_cnt   :  2
        trc_plugin_names_cnt :  {trc_plugin_names_cnt}
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
