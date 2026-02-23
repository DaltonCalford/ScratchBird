#coding:utf-8

"""
ID:          issue-6346
ISSUE:       6346
TITLE:       Connection does not see itself in the MON$ATTACHMENTS when Domain/Username (using SSPI) is 31 bytes long
DESCRIPTION:
  Could not reproduce bug on WI-V3.0.4.33054, discussed this with dimitr and alex.
  Problem can appear randomly when some byte in memory contains value not equal to 0x0.
  NB:
  1. ISC_* variables must be removed from environtment for this test properly run.
  2. Length of non-ascii user must be equal to maximal possible for tested FB: 31 for 3.x and 63 for 4.x+
JIRA:        CORE-6097
FBTEST:      bugs.core_6097
NOTES:
     [17.06.2022] pzotov
     Checked on 4.0.1.2692, 3.0.8.33535.
"""
import os
import socket
import getpass
import pytest
from firebird.qa import *
import time

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

db = db_factory(utf8filename=True)
act = python_act('db', substitutions=[('[\t ]+', ' ')])

@pytest.mark.version('>=3.0.5')
@pytest.mark.platform('Windows')
def test_1(act: Action, capsys):
    
    THIS_COMPUTER_NAME = socket.gethostname()
    CURRENT_WIN_USER = getpass.getuser()
    NON_ASCII_NAME = '"Ковалевский_Олег"' if act.is_version('<4') else '"Ковалевский_Олег_НектоПупкин_Вася"'

    map_sql = f"""
        create or alter mapping tmp_mapping_6097 using plugin win_sspi from user "{THIS_COMPUTER_NAME}\\{CURRENT_WIN_USER}" to user {NON_ASCII_NAME};
        commit;
    """
    act.isql(switches=['-q'], input=map_sql, combine_output = True)
    print(act.stdout)
    act.expected_stdout = ''
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    chk_sql = """
        set bail on;
        set count on;
        set list on;
        select 
            rdb$map_name      as map_name, 
            rdb$map_using     as map_using, 
            rdb$map_plugin    as map_plugin, 
            rdb$map_db        as map_db,
            rdb$map_from_type as map_from_type,
            rdb$map_from      as map_from, 
            rdb$map_to_type   as map_to_type, 
            rdb$map_to        as map_to
        from rdb$auth_mapping
        where rdb$map_name containing 'tmp_mapping_6097'
        ;
        select
            mon$user as who_am_i,
            -- octet_length(trim(mon$user)) as octets_in_trimmed_name, -- NB: 3.x = 31; 4.x = 78! ==> do not show it in this test; sent report to Alex et al, 17.06.2022
            left(mon$remote_protocol,3) as mon_protocol,
            left(mon$auth_method,3) as mon_auth_method
        from mon$attachments where mon$attachment_id = current_connection;
    """ 

    expected_stdout = f"""
        MAP_NAME          TMP_MAPPING_6097
        MAP_USING         P
        MAP_PLUGIN        WIN_SSPI
        MAP_DB            <null>
        MAP_FROM_TYPE     USER
        MAP_FROM          {THIS_COMPUTER_NAME}\\{CURRENT_WIN_USER}
        MAP_TO_TYPE       0
        MAP_TO            {NON_ASCII_NAME.replace('"','')}
        Records affected: 1

    	WHO_AM_I                        {NON_ASCII_NAME.replace('"','')}
    	MON_PROTOCOL                    TCP
    	MON_AUTH_METHOD                 Map
    	Records affected: 1
    """

    act.isql(switches=['-q'], input=chk_sql, combine_output = True, credentials = False)
    print(act.stdout)
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
