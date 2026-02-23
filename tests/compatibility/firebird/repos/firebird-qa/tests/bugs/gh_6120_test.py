#coding:utf-8

"""
ID:          issue-6120
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6120
TITLE:       Support auth_plugin_list dpb/spb item from application to client [CORE5860]
DESCRIPTION:
NOTES:
    [22.07.2024] pzotov
    Improvement appeared 28.08.2018 18:47, commit:
    https://github.com/FirebirdSQL/firebird/commit/dc06a58a1a7923a5954f33c8c131ae3eaf59b907

    Checked on 6.0.0.396, 5.0.1.1440, 4.0.5.3127, 3.0.12.33765
"""
import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol, DatabaseError

db = db_factory()
act = python_act('db')

tmp_srp_user = user_factory('db', name='tmp$6120_srp', password='123', plugin = 'Srp')
tmp_leg_user = user_factory('db', name='tmp$6120_leg', password='456', plugin = 'Legacy_UserManager')

@pytest.mark.version('>=3.0.3')
def test_1(act: Action, tmp_srp_user: User, tmp_leg_user: User, capsys):

    srv_cfg = driver_config.register_server(name = 'srv_cfg_6120', config = '')

    db_cfg_name = f'db_cfg_6120'
    
    # DatabaseConfig; see PYTHON_HOME/Lib/site-packages/firebird/driver/config.py:
    db_cfg_object = driver_config.register_database(name = db_cfg_name)

    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.protocol.value = NetProtocol.INET
    db_cfg_object.database.value = str(act.db.db_path)
    db_cfg_object.auth_plugin_list.value = ','.join( ['Srp', 'Legacy_Auth'] )

    for u in (tmp_srp_user, tmp_leg_user):
        with connect(db_cfg_name, user = u.name, password = u.password) as con:
            cur = con.cursor()
            try:
                cur.execute('select trim(mon$user) as mon_user, mon$auth_method as auth_way from mon$attachments')
                ccol=cur.description
                for r in cur:
                    for i in range(0,len(ccol)):
                        print( ccol[i][0],':', r[i] )
            except DatabaseError as e:
                print(e.__str__())

    act.expected_stdout = f"""
        MON_USER : TMP$6120_SRP
        AUTH_WAY : Srp

        MON_USER : TMP$6120_LEG
        AUTH_WAY : Legacy_Auth
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
