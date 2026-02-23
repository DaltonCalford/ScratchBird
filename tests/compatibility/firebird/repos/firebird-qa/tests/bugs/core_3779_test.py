#coding:utf-8

"""
ID:          issue-4123
ISSUE:       4123
TITLE:       Report OS user name in MON$ATTACHMENTS
DESCRIPTION:
  We compare values in mon$attachment with those that can be obtained using pure Python calls (without FB).
  NB: on Windows remote_os_user contains value in lower case ('zotov'), exact value was: 'Zotov'.
JIRA:        CORE-3779
FBTEST:      bugs.core_3779
"""

import pytest
import socket
import getpass
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        c.execute('select mon$remote_host, mon$remote_os_user from mon$attachments where mon$attachment_id=current_connection')
        r = c.fetchone()
        if r[0].upper() != socket.gethostname().upper():
            pytest.fail(f'FAILED check remote_host: got "{r[0]}" instead of "{socket.gethostname()}"')
        if r[1].upper() != getpass.getuser().upper():
            pytest.fail(f'FAILED check remote_os_user: got "{r[1]}" instead of "{getpass.getuser()}"')
