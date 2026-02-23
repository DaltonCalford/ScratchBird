#coding:utf-8

"""
ID:          issue-5764
ISSUE:       5764
TITLE:       New users or changed passwords in legacy authentication do not work in Firebird 4
DESCRIPTION:
  Used config:
   AuthServer = Legacy_Auth,Srp
   AuthClient = Legacy_Auth,Srp,Win_Sspi
   WireCrypt = Disabled
   UserManager = Srp, Legacy_UserManager
NOTES:
[3.11.2021] pcisar
  This test fails with 4.0, even with specified config
  Although user is created, the connect as user tmp$c5495 fails (unknown user)
JIRA:        CORE-5495
FBTEST:      bugs.core_5495
"""
import locale

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions = [('TCPv(4|6)', 'TCP'),('[ \t]+', ' ')])

tmp_user = user_factory('db', name='tmp$c5495', password='123', plugin='Legacy_UserManager')

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User):

    test_script = f"""
        set list on;
        set bail on;
        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}';
        select mon$user,mon$remote_protocol,mon$auth_method from mon$attachments
        where mon$attachment_id=current_connection;
        commit;
    """

    expected_stdout = f"""
         MON$USER                        {tmp_user.name.upper()}
         MON$REMOTE_PROTOCOL             TCP
         MON$AUTH_METHOD                 Legacy_Auth
    """
    
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout

