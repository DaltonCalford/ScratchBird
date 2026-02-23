#coding:utf-8

"""
ID:          issue-6204
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6204
TITLE:       Make WIN_SSPI plugin produce keys for wirecrypt plugin
DESCRIPTION:
    We create LOCAL mapping from current Windows user to SYSDBA and use custom driver config object ('db_cfg_object')
    in order to change DPB content and use client-side parameters: AuthClient = Win_SSpi and WireCrypt = Required.

    After this we try to connect without specifying user/password pair - it must be successful, and then
    we check that our attachment actualy uses wire encryption (by querying con.info.is_encrypted()).

    Confirmed problem on 4.0.0.1227 (date of build: 01.10.2018): attempt to connect
    using Win_SSPI leads to:
        Statement failed, SQLSTATE = 28000
        Client attempted to attach unencrypted but wire encryption is required

    Discussed with Alex, letters 23.06.2020.
    Checked on 4.0.0.1346 (date of build: 17.12.2018), SS/SC/CS - works fine.
    Checked on 3.0.6.33222, SS/SC/CS -- all OK.

JIRA:        CORE-5948
FBTEST:      bugs.core_5948
NOTES:
    [06.08.2022] pzotov
        Re-implemented: see custom driver config object ('db_cfg_object'). No more changes in the firebird.conf.
        Confirmed again problem on 4.0.0.1227.
        Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535

    [03.03.2023] pzotov
        Fixed wrong usage of driver_config.register_database() instance, added check for properly selected protocol (only INET must be used).
        Checked on 5.0.0.967 SS/CS, 4.0.3.2904 SS/CS, 3.0.11.33665 SS/CS
"""
import os
import socket
import getpass
import time

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol

#---------------------------------------
# MANDATORY! OTHERWISE ISC_ variables will take precedense over credentials = False!
for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass
#---------------------------------------

THIS_COMPUTER_NAME = socket.gethostname()
CURRENT_WIN_ADMIN = getpass.getuser()

db = db_factory()
act = python_act('db', substitutions = [('TCPv(4|6)', 'TCP')])

@pytest.mark.version('>=3.0.5')
@pytest.mark.platform('Windows')
def test_1(act: Action, capsys):

    MAP_NAME = 'test_wmap'
    addi_sql = f'''
         set bail on;
         recreate view v_map_info as
         select
              m.rdb$map_name as map_name
             ,m.rdb$map_plugin as map_plugin
             ,upper(m.rdb$map_from) as map_from
             ,m.rdb$map_to as map_to
             ,a.mon$user as who_ami
             ,a.mon$remote_protocol as att_protocol
             ,a.mon$auth_method as auth_method
         from mon$attachments a
         left join rdb$auth_mapping m on rdb$map_name = upper('test_wmap')
         where a.mon$attachment_id = current_connection
         ;
         create or alter mapping {MAP_NAME} using plugin win_sspi from user "{THIS_COMPUTER_NAME}\\{CURRENT_WIN_ADMIN}" to user {act.db.user};
         commit;
    '''
    act.isql(switches=[], input=addi_sql)

    srv_cfg = driver_config.register_server(name = 'test_srv_core_5948', config = '')
    db_cfg_name = 'test_db_core_5948'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.protocol.value = NetProtocol.INET
    db_cfg_object.database.value = str(act.db.db_path)
    db_cfg_object.config.value = f"""
        AuthClient = Win_Sspi
        WireCrypt = Required
    """
    
    # NOTE: is it mandatory to specify "user=<empty_string>" if we want to use Win_SSpi here.
    # Otherwise (if 'user' missed) driver will try to take act.db.user and connection will fail
    # with SQLSTATE = 28000 ("...user name and password are not defined...")
    #
    with connect(db_cfg_name, user = '') as con:
        prot_name = db_cfg_object.protocol.value.name if db_cfg_object.protocol.value else 'Embedded'
        with con.cursor() as cur:
             for r in cur.execute('select mon$remote_protocol as p from mon$attachments where mon$attachment_id = current_connection'):
                 assert r[0].startswith('TCP'), f'Invalid connection protocol: {r[0]}'

             for r in cur.execute('select * from v_map_info'):
                 for i,col in enumerate(cur.description):
                     print((col[0] +':').ljust(32), r[i])
        print('Is connection encrypted ?'.ljust(32),con.info.is_encrypted())

    act.expected_stdout = f"""
        MAP_NAME:                        {MAP_NAME.upper()}
        MAP_PLUGIN:                      WIN_SSPI
        MAP_FROM:                        {THIS_COMPUTER_NAME.upper()}\\{CURRENT_WIN_ADMIN.upper()}
        MAP_TO:                          {act.db.user.upper()}
        WHO_AMI:                         {act.db.user.upper()}
        ATT_PROTOCOL:                    TCP
        AUTH_METHOD:                     Mapped from Win_Sspi
        Is connection encrypted ?        True
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
