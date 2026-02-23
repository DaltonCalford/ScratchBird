#coding:utf-8

"""
ID:          issue-7001
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7001
TITLE:       ISQL showing publication status
DESCRIPTION:
NOTES:
    [08.12.2023] pzotov
    Added 'SQLSTATE' and 'error' to the list of tokens in substitutions which must NOT be filtered out.
    We have to take in account them is they occur, otherwise one can not to understand what goes wrong
    in case if test database that serves as master is absent (content of STDERR can not be saved in XML).
    Now, if db_main_alias points to non-existing file, we have to see:
        Statement failed, SQLSTATE = 08001
        I/O error during "CreateFile (open)" operation for file "db_main_alias"
        Command error: show database
    Thanks to Adriano for note with initial problem descriprion.
"""
import locale
import pytest
from firebird.qa import *

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
repl_settings = QA_GLOBALS['replication']

MAIN_DB_ALIAS = repl_settings['main_db_alias']

db_main = db_factory( filename = '#' + MAIN_DB_ALIAS, do_not_create = True, do_not_drop = True)

substitutions = [('^((?!(SQLSTATE|error|Publication:|RDB\\$DEFAULT)).)*$', ''),]
act_db_main = python_act('db_main', substitutions = substitutions)

#--------------------------------------------

@pytest.mark.replication
@pytest.mark.version('>=5.0')
def test_1(act_db_main: Action):
    test_sql = """
        set bail on;
        set list on;
        show database;        -- must include: "Publication: enabled"
        show sys pub;         -- must be: RDB$DEFAULT
        show pub rdb$default;
    """

    act_db_main.expected_stdout = """
        Publication: Enabled
        RDB$DEFAULT
        RDB$DEFAULT: Enabled, Auto-enable
    """
    act_db_main.isql(switches=['-q', '-nod'], input = test_sql, combine_output = True,io_enc = locale.getpreferredencoding())
    assert act_db_main.clean_stdout == act_db_main.clean_expected_stdout
