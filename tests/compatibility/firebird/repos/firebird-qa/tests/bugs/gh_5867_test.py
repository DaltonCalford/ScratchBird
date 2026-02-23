#coding:utf-8

"""
ID:          issue-5867
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5867
TITLE:       Add details on compression and crypt status of connection (fb_info_conn_flags) to getInfo() API call [CORE5601]
DESCRIPTION:
    Custom driver config object ('db_cfg_object') is used: WireCompression and WireCrypt are client-side parameters.
    Test checks all combinations of these parameters and compares them with values returned by query to mon$attachments.
    Also, we have to compare these values with vuts in DbInfoCode.CONN_FLAGS -- all appropriate values must be equal.
NOTES:
    [22.05.2024] pzotov
    FB 3.x is not checked: there is no ability to get info about wire* parameters from mon$attachments table.
    Checked on 6.0.0.357, 5.0.1.1404, 4.0.5.3099.
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol, DatabaseError, DbInfoCode, ConnectionFlag

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=4.0.0')
def test_1(act: Action, capsys):

    srv_cfg = driver_config.register_server(name = 'test_srv_gh_5867', config = '')
    iter = 0
    for wcompr_iter in ('True','False'):
        for wcrypt_iter in ('Disabled','Enabled'):
            iter += 1
            db_cfg_name = f'tmp_5867_wcompr_{wcompr_iter}_wcrypt_{wcrypt_iter}'
            db_cfg_object = driver_config.register_database(name = db_cfg_name)
            db_cfg_object.server.value = srv_cfg.name
            db_cfg_object.protocol.value = NetProtocol.INET
            db_cfg_object.database.value = str(act.db.db_path)
            
            db_cfg_object.config.value = f"""
                WireCompression = {wcompr_iter}
                WireCrypt = {wcrypt_iter}
            """

            dbcfg_wcompr = True if wcompr_iter == 'True' else False
            dbcfg_wcrypt = True if wcrypt_iter == 'Enabled' else False
            with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
                try:
                    cur = con.cursor()
                    cur.execute('select mon$wire_compressed, mon$wire_encrypted from mon$attachments where mon$attachment_id = current_connection')
                    mon_wire_compressed, mon_wire_encrypted = cur.fetchone()[:2]
                    con_flags_bits = con.info.get_info(DbInfoCode.CONN_FLAGS)
                    con_flags_wcompr = False if con_flags_bits & 0b01 == ConnectionFlag.NONE else True
                    con_flags_wcrypt = False if con_flags_bits & 0b10 == ConnectionFlag.NONE else True
                    
                    if dbcfg_wcompr == mon_wire_compressed and mon_wire_compressed == con_flags_wcompr \
                       and  dbcfg_wcrypt == mon_wire_encrypted and mon_wire_encrypted == con_flags_wcrypt:
                        print(f'Check # {iter}: expected.')
                    else:
                        print('Check # {iter} - MISMATCH:')
                        print(f"Set in db_cfg: WireCompression = {dbcfg_wcompr}, WireCrypt = {dbcfg_wcrypt}")
                        print(f'mon$attachments: {mon_wire_compressed=}, {mon_wire_encrypted=}')
                        print(f'DbInfoCode.CONN_FLAGS: {con_flags_wcompr=}, {con_flags_wcrypt=}')
                except DatabaseError as exc:
                    print(exc.__str__())

    act.expected_stdout = """
        Check # 1: expected.
        Check # 2: expected.
        Check # 3: expected.
        Check # 4: expected.
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
