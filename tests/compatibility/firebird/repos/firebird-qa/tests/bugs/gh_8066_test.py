#coding:utf-8

"""
ID:          issue-8066
ISSUE:       8066
TITLE:       Make protocol schemes case-insensitive
DESCRIPTION:
    Test iterates over all possible protocols (depending on OS and engine major version): INET(4|6), WNET, XNET.
    Then connection string is made using three cases of protocol string: lower, UPPER and Capitalized.
    For each kind of DSN we request mon$attachment.mon$remote_protocol value - it must correspond to DSN but 
    always must be in upper case.
    Expected output must not contain any error and must contain appropriate values for every checked protocol
    (see 'mon_remote_value')
    We have to construct this string 'on the fly' because avaliable protocols depend on OS and major version
    (see 'expected_out_lines')
NOTES:
    [06.05.2024] pzotov
    Checked on 6.0.0.344, 5.0.1.1394, 4.0.5.3091.

    [08.05.2024] pzotov
    Removed check of 'inet' (w/o digital suffix): value in mon$remote_process can be either 'TCPv4' or 'TCPv6'
    depending on Control Panel/Network and Internet/Network Connections settings.
    Connection to localhost *can* be established using IPv6 even if appropriate item has been disabled in
    network interface settings. 
    In this case 'connect inet://<alias>' causes mon$remote_address = 'TCPv4' - in contrary to 'TCPv6'
    when IPv6 is enabled.
"""

import pytest
from firebird.qa import *
from firebird.driver import NetProtocol, ShutdownMode, ShutdownMethod
import locale
import re

db = db_factory()

act = python_act('db', substitutions = [('[ \t]+', ' ')])

@pytest.mark.version('>=4.0.5')
def test_1(act: Action, capsys):

    expected_out_lines = []
    checked_dsn_column='checked_dsn_prefix'.upper()
    mon_remote_column='mon_remote_protocol'.upper()
    try:
        protocols_list = [ NetProtocol.INET4, ]

        if act.platform == 'Windows':
            protocols_list.append(NetProtocol.XNET)
            if act.is_version('<5'):
                protocols_list.append(NetProtocol.WNET)

        for p in protocols_list:
            for k in range(3):
                protocol_str = p.name.lower() if k == 0 else p.name.upper() if k==1 else p.name.title()
                mon_remote_value = 'TCPv4' if p.name.lower() == 'inet4' else p.name.upper()
                dsn = protocol_str + '://' + str(act.db.db_path) 
                test_sql = f"""
                    set bail on;
                    set list on;
                    connect {dsn};
                    select '{protocol_str}' as {checked_dsn_column}, mon$remote_protocol as {mon_remote_column} from mon$attachments where mon$attachment_id = current_connection;
                    quit;
                """
                act.isql(switches=['-q'], input = test_sql, io_enc = locale.getpreferredencoding(), combine_output = True, connect_db = False)
                expected_out_lines.append(f'{checked_dsn_column} {protocol_str}')
                expected_out_lines.append(f'{mon_remote_column} {mon_remote_value}')
                print(act.stdout)

    except Exception as e:
        print(e.__str__())

    act.expected_stdout = '\n'.join( expected_out_lines )
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
