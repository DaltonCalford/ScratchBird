#coding:utf-8

"""
ID:          syspriv.trace-any-attachment
TITLE:       Check ability to trace any attachment by non-sysdba user who is granted with necessary system privileges
DESCRIPTION:
FBTEST:      functional.syspriv.trace_any_attachment
NOTES:
   [25.05.2022] pzotov
   Test creates two user:
       1) 'tmp_syspriv_user', who has grant for trace any attachment;
       2) 'tmp_stock_manager', common non-privileged user.
   User 'tmp_syspriv_user' is granted with system privilege via role 'tmp_role_trace_any_attachment'.
   Then we launch trace by tmp_syspriv_user with making it watch for connections.
   Finally, we establish two new connections to test DB: one from non-privileged user and second from SYSDBA.
   Both of these connections must reflect in trace which was lcunched by tmp_syspriv_user.
   Checked on 4.0.1.2692, 5.0.0.497.
"""

import pytest
import locale
import re
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()
tmp_user = user_factory('db', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db', name='tmp_role_trace_any_attachment')
tmp_usr2 = user_factory('db', name='tmp_stock_manager', password='123')

act = python_act('db')

@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User, tmp_role: Role, tmp_usr2: User, capsys):

    expected_stdout = f"""
        ATTACH/1:{act.db.user} : FOUND
        ATTACH/2:{tmp_usr2.name} : FOUND
        DETACH/1:{act.db.user} : FOUND
        DETACH/2:{tmp_usr2.name} : FOUND
    """
    
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

        recreate table test_trace_any_attachment(id int);
        commit;
    """
    act.isql(switches=['-q'], input=init_script)

    trace_cfg_items = [
        'log_connections = true',
        'log_errors = true',
    ]

    with act.trace(db_events = trace_cfg_items, encoding=locale.getpreferredencoding(), user = tmp_user.name, password = tmp_user.password, role = tmp_role.name):
        # We establish two attachments (for non-priv user {tmp_usr2} and for SYSDBA).
        # BOTH of them must be seen in the trace that is generated for user {tmp_user}
        # who has apropriate system privilege:
        try:
            with act.db.connect(user = tmp_usr2.name, password = tmp_usr2.password) as con1, \
                 act.db.connect(user = act.db.user, password = act.db.password) as con2:
                pass
        except DatabaseError:
            pass

    att_ptn = re.compile( '\\)\\s+(ATTACH|DETACH)_DATABASE')
    row_bak = ''
    found_events = {}
    for line in act.trace_log:
        if att_ptn.search(row_bak):
            evt_name = 'ATTACH' if 'ATTACH' in row_bak else 'DETACH'
            evt_user = '2:'+tmp_usr2.name if tmp_usr2.name in line else '1:'+act.db.user
            found_events [ evt_name, evt_user ] = 'FOUND'
        row_bak = line

    for k,v in sorted(found_events.items()):
        print( '/'.join(k), ':',  v)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
