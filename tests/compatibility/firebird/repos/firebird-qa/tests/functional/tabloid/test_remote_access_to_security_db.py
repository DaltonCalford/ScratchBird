#coding:utf-8

"""
ID:          tabloid.remote-access-to-security-db
TITLE:       Verify ability to make REMOTE connect to security.db
DESCRIPTION:
     This test verifies only ability to make REMOTE connect to security.db
     Line "RemoteAccess = false" in file $FB_HOME/databases.conf should be COMMENTED.
     On the host that run tests this must is done BEFORE launch all testsby calling
     batch file "upd_databases_conf.bat" (see /firebirdQA/qa3x.bat; qa4x.bat).
     Checked 28.06.2016 on 4.0.0.267
FBTEST:      functional.tabloid.remote_access_to_security_db
NOTES:
    [21.08.2022] pzotov
        All avaliable remote protocols are checked (depending on OS), using custom driver_config settings.
        Checked on 5.0.0.623, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux.
    [04.03.2023] pzotov
        This test can not be executed because databases.conf is copied before every tests run
        from 'prototype': files/qa-databases.conf which has no entry for security.db

        Adding this entry (together with 'RemoteAccess = false') requires correction of batch scenarios
        that prepare databases.conf because alias 'security.db' points to the file with DIFFERENT numeric
        suffix depending on FB major version ('securityN.fdb', N = 3,4,5).
        This will be implemented later.
"""

import pytest
from firebird.qa import *
from firebird.driver import connect, driver_config, NetProtocol

db = db_factory()

act = python_act('db', substitutions = [('[ \t]+', ' '), ('TCPv(4|6)', 'TCP')] )

@pytest.mark.version('>=3.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action, capsys):
    srv_config = driver_config.register_server(name = 'test_sec_srv', config = '')
    db_cfg_object = driver_config.register_database(name = 'test_sec_db_cfg')
    db_cfg_object.database.value = 'security.db'
    db_cfg_object.server.value = 'test_sec_srv'

    sql_sttm = 'select mon$remote_protocol from mon$attachments where mon$attachment_id = current_connection'

    protocols_list = [ NetProtocol.INET, ]
    if act.platform == 'Windows':
        protocols_list.append(NetProtocol.XNET)
        if act.is_version('<5'):
            protocols_list.append(NetProtocol.WNET)

    exp_stdout_list = []
    for p in protocols_list:
        db_cfg_object.protocol.value = p
        with connect('test_sec_db_cfg', user = act.db.user, password = act.db.password) as con:
            with con.cursor() as cur:
                 for r in cur.execute(sql_sttm):
                     for i,col in enumerate(cur.description):
                         print((col[0] +':').ljust(32), r[i])
                         exp_stdout_list.append( (col[0] +':').ljust(32) + ('TCP' if p == NetProtocol.INET else (p.name if p else 'None')) )
    
    act.stdout = capsys.readouterr().out
    act.expected_stdout = '\n'.join( exp_stdout_list )
    assert act.clean_stdout == act.clean_expected_stdout
