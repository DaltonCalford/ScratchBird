#coding:utf-8

"""
ID:          issue-7239
ISSUE:       7239
TITLE:       Connect using XNET protocol shows different exception if DB is in shutdown state (comparing to INET)
DESCRIPTION:
NOTES:
    [22.02.2023] pzotov
    Confirmed bug on 4.0.2.2817, 5.0.0.592.
    Checked on 4.0.3.2903, 5.0.0.958 - all fine.
"""

import pytest
from firebird.qa import *
from firebird.driver import NetProtocol, ShutdownMode, ShutdownMethod
import locale
import re

db = db_factory()

act = python_act('db', substitutions = [('database .* shutdown', 'database shutdown')])

@pytest.mark.version('>=4.0.2')
def test_1(act: Action, capsys):

    with act.connect_server() as srv:
        srv.database.shutdown(database = act.db.db_path, mode = ShutdownMode.FULL, method = ShutdownMethod.FORCED, timeout = 0)

    try:
        protocols_list = [ NetProtocol.INET, ]
        expected_fail = """
        """
        if act.platform == 'Windows':
            protocols_list.append(NetProtocol.XNET)
            if act.is_version('<5'):
                protocols_list.append(NetProtocol.WNET)

        for p in protocols_list:
            dsn = p.name.lower() + '://' + str(act.db.db_path) 
            act.isql(switches=['-q'], input = f'connect {dsn};quit;', io_enc = locale.getpreferredencoding(), combine_output = True, connect_db = False)
            expected_fail += f"""
                {p.name.upper()}
                Statement failed, SQLSTATE = HY000
                database shutdown
            """
            print(p.name)
            print(act.stdout)
    except Exception as e:
        print(e.__str__())

    with act.connect_server() as srv:
        srv.database.bring_online(database = act.db.db_path)

    act.expected_stdout = expected_fail
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
