#coding:utf-8

"""
ID:          issue-2343
ISSUE:       2343
TITLE:       Better diagnostic when 'Missing security context'
DESCRIPTION:
    ::: NB :::
    List of AuthClient plugins must contain Win_Sspi in order to reproduce this test expected results.
    Otherwise firebird.log will not contain any message like "Available context(s): ..."
    Because of this, test marked as to be performed on WINDOWS only.
JIRA:        CORE-6362
FBTEST:      bugs.core_6362
NOTES:
    [20.06.2022] pzotov
        See also bugs/gh_7165_test.py
        Message about missed sec. context will raise if we make undefined ISC_* variables and try to connect.
        Confirmed missed info in FB 3.0.6.33301: firebird.log remains unchanged (though ISQL issues expected message).
        Checked on 4.0.1.2692, 3.0.8.33535.

    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import os
import locale
from difflib import unified_diff
import pytest
from firebird.qa import *

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

db = db_factory()

substitutions = [ ( '^((?!SQLSTATE|context).)*$', ''),
                  ( 'Missing security context(\\(s\\))?( required)? for .*', 'Missing security context'),
                  ( 'Available context(\\(s\\))?(:)? .*', 'Available context'),
                  ( '[\t ]+', ' '),
                ]

act = python_act('db', substitutions = substitutions)

expected_isql = """
    Statement failed, SQLSTATE = 28000
    Missing security context for TEST.FDB
"""

expected_fb_log_diff = """
    + Missing security context
    + Available context
"""

@pytest.mark.version('>=3.0.7')
@pytest.mark.platform('Windows')
def test_1(act: Action, capsys):
    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_init = srv.readlines()

    act.expected_stdout = expected_isql
    act.isql(switches=['-q'], input = 'quit;', credentials = False, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_curr = srv.readlines()
 

    for line in unified_diff(fb_log_init, fb_log_curr):
        if line.startswith('+'):
            print(line.strip())

    act.expected_stdout = expected_fb_log_diff
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

