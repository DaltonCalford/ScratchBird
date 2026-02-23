#coding:utf-8

"""
ID:          None
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/657d86fed65e647dc162980836d24a2e19c1342c
TITLE:       RemoteAuxPort is per-database
DESCRIPTION:
    Test checks ability to set value of free port in DPB for RemoteAuxPort parameter.
    This is done two times with verifying that value is actually changed by querying to rdb$config.
NOTES:
    [31.08.2024] pzotov
    1. No ticket has been created for this test.
    2. Custom driver-config object must be used for DPB.

    Checked on 6.0.0.4444, 5.0.2.1487, 4.0.6.3142
"""

import socket
from contextlib import closing
import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, DatabaseError

db = db_factory()
act = python_act('db')

#-----------------------------------------------------------

def find_free_port():
    # AF_INET - constant represent the address (and protocol) families, used for the first argument to socket()
    # A pair (host, port) is used for the AF_INET address family, where host is a string representing either a 
    # hostname in Internet domain notation like 'daring.cwi.nl' or an IPv4 address like '100.50.200.5', and port is an integer.
    # SOCK_STREAM means that it is a TCP socket.
    # SOCK_DGRAM means that it is a UDP socket.
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(('', 0))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        free_port = s.getsockname()[1]
    return free_port

#-----------------------------------------------------------

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    for iter in range(2):
        free_aux_port = find_free_port()
        srv_cfg = driver_config.register_server(name = f'srv_cfg_657d86fe_{iter}', config = '')
        db_cfg_name = f'db_cfg_657d86fe_{iter}'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)
        db_cfg_object.server.value = srv_cfg.name
        db_cfg_object.database.value = str(act.db.db_path)
        db_cfg_object.config.value = f"""
            RemoteAuxPort = {free_aux_port}
        """

        with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
            try:
                cur = con.cursor()
                cur.execute("select g.rdb$config_name, g.rdb$config_value from rdb$database r left join rdb$config g on g.rdb$config_name = 'RemoteAuxPort'")
                for r in cur:
                    print(iter, r[0], r[1])

            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)

        act.expected_stdout = f"""
            {iter} RemoteAuxPort {free_aux_port}
        """
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
