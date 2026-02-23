#coding:utf-8

"""
ID:          issue-7723
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7723
TITLE:       Wrong error message on login if the user doesn't exists and WireCrypt is disabled
DESCRIPTION:
    Ticket issue can be reproduced only when AuthServer = Srp256 (i.e. has default value).
    Problem *not* raises when this parameter is 'Srp' (at least, not on Windows).
    Parameter AuthServer is per-database. Because of that, we have to use separate predefined
    ALIAS with setting this parameter value to 'Srp256'.
NOTES:
    [29.08.2023] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    4. Custom driver config objects are created here, one with WireCrypt = Disabled and second with Enabled.
    5. It is supposed that no user with name 'tmp$non_existing_7723' exists.

    Confirmed on 5.0.0.1169, 4.0.4.2979
    Checked on 5.0.0.1177, 4.0.4.2982 -- all OK.
"""

import time

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol, DatabaseError

# Name of alias for self-security DB in the QA_root/files/qa-databases.conf.
# This file must be copied manually to each testing FB homw folder, with replacing
# databases.conf there:
#
REQUIRED_ALIAS = 'tmp_gh_7723_alias'

db = db_factory(filename = '#' + REQUIRED_ALIAS)
act = python_act('db')

@pytest.mark.version('>=4.0.4')
def test_1(act: Action, capsys):

    srv_cfg = driver_config.register_server(name = 'test_srv_gh_7723', config = '')
    for wc in ('Disabled', 'Enabled'):
        db_cfg_name = f'tmp_7723__wirecrypt_{wc}'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)
        db_cfg_object.server.value = srv_cfg.name
        db_cfg_object.protocol.value = NetProtocol.INET
        db_cfg_object.database.value = str(act.db.db_path)
        
        db_cfg_object.config.value = f"""
            WireCrypt = {wc}
        """
        try:
            connect(db_cfg_name, user = 'tmp$non_existing_7723', password = '123')
        except DatabaseError as exc:
            print(f"WireCrypt = {wc}. Connect failed with:") # {str(exc)}")
            for x in exc.gds_codes:
                print('* gdscode:',x)
            print('* sqlcode:', exc.sqlcode)
            print('* sqlstate:', exc.sqlstate)
            print('* text:', exc.__str__())

    act.expected_stdout = """
        WireCrypt = Disabled. Connect failed with:
        * gdscode: 335544472
        * sqlcode: -902
        * sqlstate: 28000
        * text: Your user name and password are not defined. Ask your database administrator to set up a Firebird login.

        WireCrypt = Enabled. Connect failed with:
        * gdscode: 335544472
        * sqlcode: -902
        * sqlstate: 28000
        * text: Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
