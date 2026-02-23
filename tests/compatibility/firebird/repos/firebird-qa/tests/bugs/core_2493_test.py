#coding:utf-8

"""
ID:          issue-2906
ISSUE:       2906
TITLE:       Append the IP address of the remote host to error messages in firebird.log for TCP connections
DESCRIPTION:
  Following actions are performed by this test:

  1. Obtain current firebird.log and saves it to the file with name = 'tmp_2493_fblog_before.txt';

  2. Asynchronously launch ISQL in child process with request to return client IP address (via asking context variable)
     and after this - do some 'heavy query' that for sure will take a lot of time and resources.
     Output is redirected to file with name = 'tmp_2493_isql.log' and will be parsed further (we'll search for client IP there).

  3. Kill launched ISQL process after several seconds. At this point new message must appear in firebird.log and it MUST
     be in format described in the ticket. Because this abend will be detected by SERVER, format of message will be like this:
     (for TCPv4):  INET/inet_error: read errno = 10054, client host = prog1, address = 127.0.0.1/4076, user = john
     (for TCPv6):  INET/inet_error: read errno = 10054, client host = prog2, address = fe80::c40e:21ec:b5c7:8963/56831, user = mick

  4. Wait several seconds and after it - obtain again firebird.log (new content) and save it in 'tmp_2493_fblog_after.txt'.

  5. Make file comparison by calling method from standard Python tool - difflib. Result of this comparison will be stored
     in file with name 'tmp_2493_diff.txt'. This file will have several lines from which we are interested only for one which
     STARTS with "+" (PLUS sign) and does contain phrase 'INET/INET_ERROR'. Diff-file must contain only ONE such line.

  6. Next we parse this line: remove "/" and "="characters from it and split then text into array of words:
     + INET inet_error read errno 10054 client host prog1 address 127.0.0.1 4417   user john ------- for IPv4
     0  1       2       3    4      5     6     7     8       9       10      11     12   13
     + INET inet_error read errno 10054 client host prog2 address x::y:z:u:v 56831 user mick ------- for IPv6
  7. Then we scan this array backward and check tokens for matching simple rules (N = array len):
     * token N-1 must be OS user name; this name can be followed by some kind of "suffix": "JOHN.-1.-1" - and we have to take only 1st word from it.
       NB: we current OS user using call of getpass.getuser(). It must be compared in case insensitive manner;
     * token N-2 is just word "user" (as is);
     * token N-3 is port number, it has to be positive value;
     * token N-4 is IP. It must be equal to rdb$get_context('SYSTEM','CLIENT_ADDRESS'). / fe80::a408:4a80:5496:1548%8/49569

     This is how differences look in firebird.log:
     # 2.5.9:
     #       INET/inet_error: read errno = 10054, client address = 127.0.0.1 3268, user ZOTOV.-1.-1
     #                                                                   ^    ^    ^      ^
     #                                                                  N-4  N-3  N-2    N-1
     # 3.0.4:
     #       INET/inet_error: read errno = 10054, client host = csprog, address = 127.0.0.1 3298, user zotov
     #                                                                                 ^     ^     ^      ^
     #                                                                                N-4   N-3   N-2   N-1
     # 3.0.8 and 4.0.0 RC1:
     #       INET/inet_error: read errno = 10054, client host = csprog, address = fe80::fcf1:e33c:e924:969d%16/56887, user = zotov
     #       INET/inet_error: read errno = 10054, client host = csprog, address = fe80::fcf1:e33c:e924:969d%16/56883, user = zotov
NOTES:
    [20.02.2021] pzotov
      changed 'platform' attribute to Windows only. Content of firebird.log has no changes on Linux during this test run.
      Perhaps, this is temporary and another solution will be found/implemented. Sent letter to dimitr et al, 21.02.2021 08:20.
    [27.05.2022] pzotov
      Re-implemented for work in firebird-qa suite. 
      Checked on: 3.0.8.33535, 4.0.1.2692, 5.0.0.497

JIRA:        CORE-2493
FBTEST:      bugs.core_2493
"""

import os
import subprocess
import pytest
import locale
import time
import getpass
from pathlib import Path
from firebird.qa import *
from firebird.driver import ShutdownMode,ShutdownMethod
from firebird.driver.types import DatabaseError
from difflib import unified_diff

#--------------------------------------------

# http://stackoverflow.com/questions/319279/how-to-validate-ip-address-in-python
def is_valid_ipv4(address):
    import socket
    try:
        socket.inet_pton(socket.AF_INET, address)
    except AttributeError:  # no inet_pton here, sorry
        try:
            socket.inet_aton(address)
        except socket.error:
            return False
        return address.count('.') == 3
    except socket.error:  # not a valid address
        return False

    return True

#--------------------------------------------

def is_valid_ipv6(address):
    import socket
    try:
        socket.inet_pton(socket.AF_INET6, address)
    except socket.error:  # not a valid address
        return False
    return True

#--------------------------------------------

init_script = """
    create table tlog(ip varchar(255));
    create table tdelay(id int primary key);
    set term ^;
    create procedure sp_delay as
    begin
        insert into tdelay(id) values(1);
        in autonomous transaction do
        begin
            execute statement ('insert into tdelay(id) values(?)') (1);
            when any do
            begin
                -- nop --
            end
        end
        delete from tdelay where id = 1;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init = init_script)
tmp_user = user_factory('db', name='tmp_user_2493', password='123')
act = python_act('db')

tmp_paused_sql = temp_file('tmp_paused_2493.sql')
tmp_paused_log = temp_file('tmp_paused_2493.log')

expected_stdout_log_diff = """
    Check IP using methods from 'socket' package: PASSED.
    Check IP for equality to 'CLIENT_ADDRESS' value: PASSED.
    Check port value: PASSED, positive integer.
    Check OS user using 'getpass' package: PASSED
"""

@pytest.mark.version('>=3')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_paused_sql: Path, tmp_paused_log: Path, capsys):

    mon_ip = 'UNKNOWN_IP'
    with act.db.connect() as con1:
        cur1 = con1.cursor()
        cur1.execute("select rdb$get_context('SYSTEM','CLIENT_ADDRESS') from rdb$database")
        mon_ip = cur1.fetchone()[0].split('/')[0] # '<ip_address>/<port>' --> '<ip_address>' etc

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_init = srv.readlines()

    tmp_paused_sql.write_text("rollback;set transaction lock timeout 7;execute procedure sp_delay;")

    with open(os.devnull, 'w') as fn_nul:
    #with open(tmp_paused_log, mode='w') as fn_nul:
        p_paused_isql = subprocess.Popen([act.vars['isql'], act.db.dsn,
                                          '-user', act.db.user,
                                          '-password', act.db.password,
                                          '-q', '-i', str(tmp_paused_sql)],
                                          stdout = fn_nul,
                                          stderr = subprocess.STDOUT
                                        )
        time.sleep(1) # Let ISQL to establish connection and stay there in pause
        p_paused_isql.terminate()


    time.sleep(1) # Let changes in firebird.log be flushed on disk

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_curr = srv.readlines()

    inet_msg_words = []
    for line in unified_diff(fb_log_init, fb_log_curr):
        if line.startswith('+') and 'INET/INET_ERROR' in line.upper():
            # DO NOT include ':' in list of delimiters! It occurs in IPv6 address:
            inet_msg_words = line.replace(',',' ').replace('/',' ').replace('=',' ').split()
            break

    # Tokens, numerated from zero (NB: leftmost is "PLUS" sign and has index = 0)
    # ---------------------------------------------------------------------------
    # + INET inet_error read errno 10054 client host prog1 address 127.0.0.1   4417 user john ------- for IPv4
    # + INET inet_error read errno 10054 client host prog2 address x::y:z:u:v 56831 user mick ------- for IPv6
    # 0  1        2       3     4    5     6      7    8      9       10        11   12   13

    n = len(inet_msg_words)

    parsing_problem_msg = 'Problem with parsing content of firebird.log'
    if len(inet_msg_words) == 0:
        print(f'{parsing_problem_msg}: message with "inet_error" not found.')
    elif len(inet_msg_words) < 4:
        print(f'{parsing_problem_msg}: message with "inet_error" contains less than 4 tokens.')
    else:

        # http://stackoverflow.com/questions/4271740/how-can-i-use-python-to-get-the-system-hostname

        if is_valid_ipv4(inet_msg_words[n-4]) or is_valid_ipv6(inet_msg_words[n-4]):
            print("Check IP using methods from 'socket' package: PASSED.")
        else:
            print("IP address is INVALID.")

        if inet_msg_words[n-4] == mon_ip:
            print("Check IP for equality to 'CLIENT_ADDRESS' value: PASSED.")
        else:
            print('IP address: |'+inet_msg_words[n-4]+'| - differ from mon_ip=|'+mon_ip+'|')

        if inet_msg_words[n-3].isdigit():
            print('Check port value: PASSED, positive integer.')
        else:
            print('Invalid port=|'+inet_msg_words[n-3]+'|')

        if inet_msg_words[n-1].upper().split('.')[0] == getpass.getuser().upper():
            # 2.5.9: got 'ZOTOV.-1.-1' ==> must be just one word: 'ZOTOV'
            print("Check OS user using 'getpass' package: PASSED")
        else:
            print('Invalid OS user=|'+inet_msg_words[n-1]+'|')


    act.expected_stdout = expected_stdout_log_diff
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
