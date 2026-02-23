#coding:utf-8

"""
ID:          issue-8520
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8520
TITLE:       Error in iTransaction.getInfo() on embedded connection
DESCRIPTION:
NOTES:
    [17.04.2025] pzotov
    Confirmed problem on 6.0.0.686 (20-mar-2025).
    Checked on 6.0.0.737-cf1d367, intermediate snapshot.

    [18.04.2025] pzotov
    Reduced min_version after check on iontermediate snapshots:
    5.0.3.1648-ca2f3e7; 4.0.6.3199-0503997; 3.0.13.33807-5d3394e7
"""

import os
import re
import time

import pytest
from firebird.qa import *
from firebird.driver import connect, driver_config, NetProtocol, DatabaseError

db = db_factory()
act = python_act('db')

#-----------------------------------------------------------

@pytest.mark.version('>=3.0.13')
def test_1(act: Action, capsys):

    srv_config = driver_config.register_server(name = 'test_8520_srv', config = '')
    db_cfg_object = driver_config.register_database(name = 'test_8520_cfg')
    db_cfg_object.database.value = str(act.db.db_path)
    db_cfg_object.server.value = srv_config.name

    sql_sttm = 'select mon$remote_protocol from mon$attachments where mon$attachment_id = current_connection'

    protocols_list = [ None, NetProtocol.INET, ] # None - for local/embedded connection.
    if act.platform == 'Windows':
        protocols_list.append(NetProtocol.XNET)
        if act.is_version('<5'):
            protocols_list.append(NetProtocol.WNET)
    
    expected_out_map = {}
    for p in protocols_list:
        db_cfg_object.protocol.value = p
        with connect(db_cfg_object.name, user = act.db.user, password = act.db.password) as con:
            protocol_name = 'UNKNOWN'
            with con.cursor() as cur:
                 for r in cur.execute(sql_sttm):
                     protocol_name = 'NONE' if p == None else p.name.upper()
            try:
                with con.main_transaction as tr:
                    expected_out_map[ protocol_name ] = tr.info.database
            except DatabaseError as e:
                print(f'Error encountered for {protocol_name=}:')
                print(e.__str__())
                print(e.gds_codes)
    
    # Construct expected output + print actual result for comparison with expected one:
    expected_out_lst = []
    for k,v in expected_out_map.items():
        print(k.lower(), v.lower())
        expected_out_lst.append( (k + ' ' +  ('' if k == 'NONE' else k +'://') + str(act.db.db_path)).lower() )

    expected_stdout = '\n'.join(expected_out_lst)
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
