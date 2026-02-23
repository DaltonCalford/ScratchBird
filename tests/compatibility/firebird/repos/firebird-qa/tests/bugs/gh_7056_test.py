#coding:utf-8

"""
ID:          issue-8168
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8168
TITLE:       Fetching from a scrollable cursor may overwrite user-specified buffer and corrupt memory
DESCRIPTION:
    Engine did overwrite the user-specified buffer with four more bytes than expected that could corrupt the caller memory.
    Discussed between dimitr, pcisar and pzotov, see letters of 29-30 NOV 2021,
    subj: "firebird-driver & scrollable cursors // misc. tests, requested by dimitr"
NOTES:
    [18.07.2024] pzotov
    1. ### ACHTUNG ###
       Old snapshots (before 5.0.0.890-aa847a7) must be checked with usage "--disable-db-cache" command switch for pytest!
       Otherwise one may FALSE failure (bugcheck) with:
       "internal Firebird consistency check (decompression overran buffer (179), file: sqz.cpp line: 293)"

    2. Initial problem related to cursor operation was found in 5.0.0.316 and result depends on CONNECTION PROTOCOL(!):
       1.1. For REMOTE protocol cur.fetch_first() raised "firebird.driver.types.DatabaseError: feature is not supported"
       1.2. For LOCAL protocol Python crashed with console output:
       ==============
           Current thread 0x00004ad0 (most recent call first):
             File "C:/FBTESTING/qa/firebird-qa/tests/bugs/gh_7056_test.py", line 74 in test_1
             File "C:/Python3x/Lib/site-packages/_pytest/python.py", line 194 in pytest_pyfunc_call
             File "C:/Python3x/Lib/site-packages/pluggy/_callers.py", line 102 in _multicall
             ...
             File "C:/Python3x/Lib/site-packages/_pytest/config/__init__.py", line 198 in console_main
             File "C:/Python3x/Scripts/pytest.exe/__main__.py", line 7 in <module>
             File "<frozen runpy>", line 88 in _run_code
             File "<frozen runpy>", line 198 in _run_module_as_main
       ==============

       Problem with "firebird.driver.types.DatabaseError: feature is not supported" has been fixed in 5.0.0.320, commit:
           5a5a2992f78c1af9408091a0bd3fff50e0bc5d6a (26-nov-2021 09:11)
           "Better boundary checks, code unification, removed end-of-stream errors when fetching past EOF / beyond BOF (as per SQL spec)"
       Problem with Python crash did exist up to 5.0.0.325 (30-nov-2021) and has been fixed in 5.0.0.326-fd6bf8d (01-dec-2021 08:44)
    3. Problem appeared only for column with width = 32765 characters, thus DB charset must be single-byte, e.g. win1251 etc.
       Otherwise (with default charset = 'utf8') this test will fail with:
       "SQLSTATE = 54000 / ... or string truncation / -Implementation limit exceeded"
    4. Custom driver-config object must be used for DPB because TWO protocols are checked here: LOCAL and REMOTE.

    Checked on 6.0.0.396, 5.0.1.1440
"""
import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol, DatabaseError
import re

N_WIDTH = 32765

init_script = f"""
    create table ts(id int primary key, s varchar({N_WIDTH}));
    insert into ts(id,s) values( 1, lpad('', {N_WIDTH}, 'A') );
    commit;
"""
db = db_factory(init=init_script, charset = 'win1251')
act = python_act('db', substitutions=[('[ \t]+', ' ')])

def strip_white(value):
    value = re.sub('(?m)^\\s+', '', value)
    return re.sub('(?m)\\s+$', '', value)

@pytest.mark.scroll_cur
@pytest.mark.version('>=5.0.0')
def test_1(act: Action, capsys):
    srv_cfg = driver_config.register_server(name = 'test_srv_gh_7056', config = '')
    actual_out = expected_out = ''
    for protocol_name in ('local', 'remote'):
        
        db_cfg_name = f'tmp_7056_{protocol_name}'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)
        db_cfg_object.server.value = srv_cfg.name
        db_cfg_object.protocol.value = None if protocol_name == 'local' else NetProtocol.INET
        db_cfg_object.database.value = str(act.db.db_path)

        success_msg = f'Protocol: {protocol_name} - COMPLETED.'
        expected_out += success_msg + '\n'

        with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
            try:
                cur = con.cursor()
                cur.open('select id, s from ts order by id')
                cur.fetch_first()
                print(success_msg)
            except DatabaseError as e:
                print(e.__str__())

        actual_out += capsys.readouterr().out + '\n'

    assert expected_out != '' and strip_white(actual_out) == strip_white(expected_out)
