#coding:utf-8

"""
ID:          issue-8554
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8554
TITLE:        Firebird XDR Message Parsing NULL Pointer Dereference Denial-of-Service Vulnerability
DESCRIPTION:
NOTES:
    [03.09.2025] pzotov
    More details:
        https://www.zerodayinitiative.com/advisories/published/
        (GHSA-7qp6-hqxj-pjjp / ZDI-CAN-26486)
        https://www.cve.org/CVERecord?id=CVE-2025-54989
    Fixed on:
        https://github.com/FirebirdSQL/firebird/commit/169da595f8693fc1a65a79c741724b1bc8db9f25

    [29.09.2025] pzotov
    Number of listening port must be pre-checked before further actions in order to use default
    value (3050). Otherwise "TypeError: int() ... must be ... not 'NoneType'" raises if QA plugin
    works under standard configuration which has no explicitly specified port.
    Thanks to Anton Zuev, Redbase.

    Confirmed bug on: 6.0.0.767; 5.0.3.1650; 4.0.6.3200; 3.0.13.33808
    Checked on: 6.0.0.770; 5.0.3.1651; 4.0.6.3203; 3.0.13.33809
"""

import socket
from binascii import unhexlify

import pytest
from firebird.qa import *

db = db_factory()

substitutions = [('Received \\d+ bytes', 'Received N bytes')]
act = python_act('db', substitutions = substitutions)

BUFFER_SIZE = 1024
SOCKET_TIMEOUT = 5

@pytest.mark.version('>=3.0.13')
def test_1(act: Action, capsys):

    # Define outbound data
    outbound_data = [
        b'\x00\x00\x00\x01\x00\x03\x00\x00\x00\x00\x00\x03\x00\x00\x00\x1d\x00\x00\x00\x00\x00\x00\x00\t\x00\x00\x01P\t\x0bxxxxxxadmin\x08\x06Srp256\n"Srp256, Srp, Win_Sspi, Legacy_Auth\x07\xff\x00F1331B2A2AAA4F62CF94CC5226D9DEDD29C8D4AB5D8491649B240402505954CA113E4D499BE17A8644691A3D7DD7C01837B086D9E0517C4A90D5CD7602500B97B83980E22C49E3BFFF1031E689A809BE71F0DB4FF1C0C6B38CB1AC18015F5F85ADB8D9DE4C7E1308F240FCF4975541E417CEBD576D3C08C99E88EB63E9DC59\x0b\x04\x01\x00\x00\x00\x01\x02os\x04\x08jin-dell\x06\x00\x00\x00\x00\n\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x02\xff\xff\x80\x0b\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x04\xff\xff\x80\x0c\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x06\xff\xff\x80\r\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x08\xff\xff\x80\x0e\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\n\xff\xff\x80\x0f\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x0c\xff\xff\x80\x10\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x0e\xff\xff\x80\x11\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x10\xff\xff\x80\x12\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x00\x00\x00\x12',
        b'\x00\x00\x00R\x00\x00\x00\x00\x00\x00\x00\x0bservice_mgr\x00\x00\x00\x00\x82\x02\x02v\x00\x1e\x0bjygjCIf.YHg\x1c\x0bxxxxxxadmin:\x04\x00\x00\x00\x00n\x04\xc0\x90\x00\x00pVD:\\AAAAAAAAAAAA\\AAAAAA\\AAAAAAAAAAAA\\Firebird\\firebird_maestro_executable\\FbMaestro.exe\x00\x00',
        b'\x00\x00\x00U\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x07\x00\x00\x00',
        b'\x00\x00\x00T\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01D\x00\x00\x00\x00\x00}\x00',
        b'\x00\x00\x00S\x00\x00\x00\x00',
        b'\x00\x00\x00PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP\x06' # this is fuzzy data
    ]

    # Define the server address and port number
    if act.vars.get('port'):
        fb_port = int(act.vars['port'])
    else:
        fb_port = 3050

    server_address = ('localhost', fb_port)

    # Creating a TCP Client Socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
        # Loop to send outbound data and receive inbound data
        try:
            client_socket.settimeout(SOCKET_TIMEOUT)
            client_socket.connect(server_address)
            for i,data in enumerate(outbound_data):
                # Send data
                print(f"Item {i}. Trying to send.")
                client_socket.sendall(data)
                print(f"Item {i}. Sent completed.")

                # receive data
                print(f"Item {i}. Trying to receive.")
                received_data = client_socket.recv(BUFFER_SIZE)
                #print("Received:", binascii.hexlify(received_data))
                print(f"Item {i}. Received {len(received_data)} bytes")
        except ConnectionError as e:
            # ConnectionResetError: [WinError 10054] An existing connection was forcibly closed by the remote hos
            # ConnectionRefusedError: [WinError 10061] No connection could be made because the target machine actively refused it
            print(f'{e.__class__=}')
            print(f'{e.errno=}')
            # DISABLED OUTPUT: localized message can be here! >>> print(e) 
        except Exception as x:
            print("### ERROR ###")
            print(f'{x.__class__=}')

    act.expected_stdout = """
        Item 0. Trying to send.
        Item 0. Sent completed.
        Item 0. Trying to receive.
        Item 0. Received 36 bytes
        Item 1. Trying to send.
        Item 1. Sent completed.
        Item 1. Trying to receive.
        Item 1. Received 288 bytes
        Item 2. Trying to send.
        Item 2. Sent completed.
        Item 2. Trying to receive.
        Item 2. Received 32 bytes
        Item 3. Trying to send.
        Item 3. Sent completed.
        Item 3. Trying to receive.
        Item 3. Received 32 bytes
        Item 4. Trying to send.
        Item 4. Sent completed.
        Item 4. Trying to receive.
        Item 4. Received 32 bytes
        Item 5. Trying to send.
        Item 5. Sent completed.
        Item 5. Trying to receive.
        Item 5. Received 0 bytes
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
