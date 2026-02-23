#coding:utf-8

"""
ID:          issue-5508
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5508
TITLE:       Allow to enforce IPv4 or IPv6 in URL-like connection strings
DESCRIPTION:
JIRA:        CORE-5229
FBTEST:      bugs.core_5229
NOTES:
    [04.02.2022] pcisar
        Test may fail with IPv6.
        For example it fails on my Linux OpenSuSE Tumbleweed with regular setup (IPv6 should not be disabled).
        Test should IMHO check IPv4/IPv6 availability on test host before runs inet6:// check.
    [13.06.2024] pzotov
        1. Added check for ability to use IPv6.
        2. Attempt to specify explicitly IPv6 address "[::1]" in ES/EDS caused error:
           ========
           Statement failed, SQLSTATE = 42000
           External Data Source provider 'inet6://[' not found
           ========
           It was fixed in gh-8156.
        3. On Windows there is no way to make IPv6 'fully disabled': address '::1' remains active.
           According to https://learn.microsoft.com/en-us/troubleshoot/windows-server/networking/configure-ipv6-in-windows
           "You cannot completely disable IPv6 as IPv6 is used internally on the system for many TCPIP tasks.
           For example, you will still be able to run ping ::1 after configuring this setting"

           We can turn off listening of '::1' by FB server if do following:
               * run PowerShell and type there: Enable-NetAdapterBinding -Name "*" -ComponentID ms_tcpip6
               * chcp 65001, then: ipconfig /all | findstr /i /r /c:" IPv6.*(preferred)"
               * save somewhere IPv6 address from previous command (e.g. 'fe80::f53c:9ecf:aad:4761%14')
               * change in firebird.conf: RemoteBindAddress = fe80::f53c:9ecf:aad:4761
           But this requires RESTART of FB server thus cannot be used in QA.
           Discussed with Vlad 13-jun-2024.
    [14.06.2024] pzotov
        Checked "on external 'inet6://[::1]/{act.db.db_path}'" after fixed GH-8156, builds:
        3.0.12.33757, 4.0.5.3112, 5.0.1.1416, 6.0.0.374
"""
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

#------------------------------------------
# https://stackoverflow.com/questions/66246308/detect-if-ipv6-is-supported-os-agnostic-no-external-program/66249915#66249915
# https://stackoverflow.com/a/66249915

def check_ipv6_avail():
    import socket
    import errno

    # On Windows, the E* constants will use the WSAE* values
    # So no need to hardcode an opaque integer in the sets.
    _ADDR_NOT_AVAIL = {errno.EADDRNOTAVAIL, errno.EAFNOSUPPORT}
    _ADDR_IN_USE = {errno.EADDRINUSE}

    res = -1
    if not socket.has_ipv6:
        # If the socket library has no support for IPv6, then the
        # question is moot as we can't use IPv6 anyways.
        return res

    sock = None
    try:
        #with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as sock:
        #    sock.bind(("::1", 0))
        sock = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
        sock.bind(("::1", 0))
        #sock.shutdown(socket.SHUT_RDWR) # [Errno 107] Transport endpoint is not connected
        sock.close()
        res = 0
    except socket.error as x:
        # sysctl net.ipv6.conf.all.disable_ipv6=1
        # sysctl net.ipv6.conf.default.disable_ipv6=1
        # sock.bind(("::1", 0)) --> socket.error: [Errno 99] Cannot assign requested address
        #print(x)
        res = -2
    except OSError as e:
        if e.errno in _ADDR_NOT_AVAIL:
            res = -3
        elif e.errno in _ADDR_IN_USE:
            # This point shouldn't ever be reached. But just in case...
            res = -4
        else:
            # Other errors should be inspected
            res = -5

    return res
#------------------------------------------

@pytest.mark.es_eds
@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    
    if (res := check_ipv6_avail()) < 0:
        pytest.skip(f"IPv6 not avail, retcode: {res}")

    sql_chk = f"""
        set list on;
        commit;
        connect 'inet4://127.0.0.1/{act.db.db_path}';

        select mon$remote_protocol as procotol_when_connect_from_isql
        from mon$attachments where mon$attachment_id = current_connection;

        set term ^;
        execute block returns(protocol_when_connect_by_es_eds varchar(20) ) as
            declare stt varchar(255) = 'select mon$remote_protocol from mon$attachments where mon$attachment_id = current_connection';
        begin
            for
                execute statement (stt)
                    on external 'inet4://127.0.0.1/{act.db.db_path}'
                    as user '{act.db.user}' password '{act.db.password}'
                into protocol_when_connect_by_es_eds
            do
                suspend;
        end
        ^
        set term ;^
        commit;

        -- since 27.10.2019; checked again 13.06.2024
        connect 'inet6://[::1]/{act.db.db_path}';

        select mon$remote_protocol as procotol_when_connect_from_isql
        from mon$attachments where mon$attachment_id = current_connection;

        set term ^;
        execute block returns(protocol_when_connect_by_es_eds varchar(20) ) as
            declare stt varchar(255) = 'select mon$remote_protocol from mon$attachments where mon$attachment_id = current_connection';
        begin
            for
                execute statement (stt)
                    -- Failed before fix #8156 ("Can not specify concrete IPv6 address in ES/EDS connection string"):
                    on external 'inet6://[::1]/{act.db.db_path}'
                    as user '{act.db.user}' password '{act.db.password}'
                into protocol_when_connect_by_es_eds
            do
                suspend;
        end
        ^
        set term ;^
        commit;
    """

    expected_stdout = """
        PROCOTOL_WHEN_CONNECT_FROM_ISQL TCPv4
        PROTOCOL_WHEN_CONNECT_BY_ES_EDS TCPv4
        PROCOTOL_WHEN_CONNECT_FROM_ISQL TCPv6
        PROTOCOL_WHEN_CONNECT_BY_ES_EDS TCPv6
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q', f'inet://{act.db.db_path}'], input=sql_chk, connect_db=False)
    assert act.clean_stdout == act.clean_expected_stdout

