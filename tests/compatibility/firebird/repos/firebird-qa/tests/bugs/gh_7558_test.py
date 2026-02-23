#coding:utf-8

"""
ID:          issue-6875
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7558
TITLE:       AV in engine when attaching to the non-existing database and non-SYSDBA trace session is running
DESCRIPTION:
NOTES:
    [22.05.2023] pzotov
        Confirmed crash on 4.0.3.2933, got on attempt to make connection:
            Error reading data from the connection.
            (335544726,)
        Checked on 4.0.3.2936 SS/CS - works OK, no crash.
    [14.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
import locale
import re
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()
db_non_existing_database = db_factory(filename = 'no_such_file.tmp', do_not_create = True, do_not_drop = True)

tmp_user = user_factory('db', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db', name='tmp_role_trace_any_attachment')
tmp_usr2 = user_factory('db', name='tmp_stock_manager', password='123')

substitutions = [('^((?!SQLSTATE|(I/O error)|(Error while)|335544344|335544734).)*$', ''), ('CreateFile\\s+\\(open\\)', 'open')]

act = python_act('db', substitutions = substitutions)
act_non_existing_database = python_act('db_non_existing_database')

@pytest.mark.trace
@pytest.mark.version('>=4.0.3')
def test_1(act: Action, act_non_existing_database: Action, tmp_user: User, tmp_role: Role, tmp_usr2: User, capsys):
  
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
    assert act.clean_stdout == ''
    act.reset()

    trace_cfg_items = [
        'log_connections = true',
        'log_errors = true',
    ]

    with act.trace(db_events = trace_cfg_items, encoding=locale.getpreferredencoding(), user = tmp_user.name, password = tmp_user.password, role = tmp_role.name):
        # We establish two attachments (for non-priv user {tmp_usr2} and for SYSDBA).
        # BOTH of them must be seen in the trace that is generated for user {tmp_user}
        # who has apropriate system privilege:
        try:
            with act_non_existing_database.db.connect(user = tmp_user.name, password = tmp_user.password) as con1:
                pass
        except DatabaseError as e:
            print(e.__str__())
            for g in e.gds_codes:
                print(g)

            # WINDOWS: I/O error during "CreateFile (open)" operation for file "..."
            # LINUX:   I/O error during "open" operation for file "..."
            # -Error while trying to open file
            # -[ localized message here: "no such file" ]
            # (335544344, 335544734)

    expected_stdout = f"""
        I/O error during "open" operation for file "{act_non_existing_database.db.db_path}"
        -Error while trying to open file
        335544344
        335544734
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout.lower() == act.clean_expected_stdout.lower()
