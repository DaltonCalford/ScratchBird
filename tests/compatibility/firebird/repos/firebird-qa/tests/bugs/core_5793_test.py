#coding:utf-8

"""
ID:          issue-6056
ISSUE:       6056
TITLE:       Error returned from DbCryptPlugin::setKey() is not shown
DESCRIPTION:
    Test issues: 'alter database encrypt ...' with NON existing key and check exception that will raise.

JIRA:        CORE-5793
FBTEST:      bugs.core_5793
NOTES:
    [12.06.2022] pzotov
    Checked on 4.0.1.2692, 3.0.8.33535 - both on Linux and Windows.

    [31.07.2022] pzotov
    Test reads settings that are COMMON for all encryption-related tests and stored in act.files_dir/test_config.ini.
    QA-plugin prepares this by defining dictionary with name QA_GLOBALS which reads settings via ConfigParser mechanism.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

act = python_act('db')

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
enc_settings = QA_GLOBALS['encryption']

ENCRYPTION_PLUGIN = enc_settings['encryption_plugin'] # fbSampleDbCrypt
ENCRYPTION_HOLDER = enc_settings['encryption_holder'] # 'fbSampleKeyHolder'
ENCRYPTION_BADKEY = enc_settings['encryption_badkey'] # 'NoSuchKey'


fb3x_checked_stdout = f"""
    unsuccessful metadata update
    -ALTER DATABASE failed
    -Missing correct crypt key
    -Plugin {ENCRYPTION_HOLDER}:
    -Crypt key {ENCRYPTION_BADKEY} not set
"""

fb4x_checked_stdout = f"""
    unsuccessful metadata update
    -ALTER DATABASE failed
    -Missing database encryption key for your attachment
    -Plugin {ENCRYPTION_HOLDER}:
    -Crypt key {ENCRYPTION_BADKEY} not set
"""

@pytest.mark.encryption
@pytest.mark.version('>=3.0.4')
def test_1(act: Action, capsys):
    
    with act.db.connect() as con:
        sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_BADKEY}"'
        try:
            con.execute_immediate(sttm)
        except DatabaseError as e:
            print( e.__str__() )

        act.expected_stdout = fb3x_checked_stdout if act.is_version('<4') else fb4x_checked_stdout
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout, f'Failed command: {sttm}'
