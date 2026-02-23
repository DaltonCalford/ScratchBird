#coding:utf-8

"""
ID:          issue-4386
ISSUE:       4386
TITLE:       Remote Stack Buffer Overflow in Firebird SQL Server (when specially crafted packet is sent via socket API)
DESCRIPTION:
JIRA:        CORE-4058
FBTEST:      bugs.core_4058
"""

import pytest
import socket
from binascii import unhexlify
from difflib import unified_diff
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.connect_server() as srv:
        srv.info.get_log()
        log_before = srv.readlines()
        # Extract port from firebird.conf
        fb_home = Path(srv.info.home_directory)
        fb_config: Path = fb_home / 'firebird.conf'
        for line in fb_config.read_text().splitlines():
            if 'remoteserviceport' in line.lower() and '=' in line:
                fb_port = line.split('=')[1].strip()
        # Send crafted packet
        data_1 =  b""
        data_1 += b"00000001000000130000000200000024"
        data_1 += b"00000010433a5c746573745f66697265"
        data_1 += b"626972640000000400000022"
        data_1 += b"0510"
        data_1 += b"41414141424242424343434344444444"
        data_1 += b"05156c6f63616c"
        data_1 += b"686f73742e6c6f63616c646f6d61696e"
        data_1 += b"06000000000000090000000100000002"
        data_1 += b"00000005000000020000000a00000001"
        data_1 += b"000000020000000500000004ffff800b"
        data_1 += b"00000001000000020000000500000006"
        data_1 += b"000000010000000200000005"
        data_1 += b"0000000800"

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect(('localhost', int(fb_port)))
            s.send(unhexlify(data_1))
            s.close()
        #
        srv.info.get_log()
        log_after = srv.readlines()
        #
        assert list(unified_diff(log_before, log_after)) == []
