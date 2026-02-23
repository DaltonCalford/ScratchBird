#coding:utf-8

"""
ID:          issue-7425
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7425
TITLE:       Add REPLICA MODE to the output of the isql SHOW DATABASE command
DESCRIPTION:
NOTES:
    [20.04.2023] pzotov
        Currently avaliable only in Firebird-4.x. Waiting for frontport.
        Checked on 4.0.3.2931
    [14.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Removed 'pytest.skip' for FB 5.x+ because this feature was frontported to these FB versions.
"""
import locale
import pytest
from firebird.qa import *

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
repl_settings = QA_GLOBALS['replication']

MAIN_DB_ALIAS = repl_settings['main_db_alias']
REPL_DB_ALIAS = repl_settings['repl_db_alias']

db_main = db_factory( filename = '#' + MAIN_DB_ALIAS, do_not_create = True, do_not_drop = True)
db_repl = db_factory( filename = '#' + REPL_DB_ALIAS, do_not_create = True, do_not_drop = True)

substitutions = [('^((?!(SQLSTATE|Replica mode:)).)*$', ''),]

act_db_main = python_act('db_main', substitutions = substitutions)
act_db_repl = python_act('db_repl', substitutions = substitutions)

#--------------------------------------------

@pytest.mark.replication
@pytest.mark.version('>=4.0.3')
def test_1(act_db_main: Action,  act_db_repl: Action, capsys):

    for a in (act_db_main, act_db_repl):
        a.expected_stdout = 'Replica mode: ' + ('NONE' if a == act_db_main else 'READ_ONLY')
        a.isql(switches=['-q', '-nod'], input = 'show database;', combine_output = True, io_enc = locale.getpreferredencoding())
        assert a.clean_stdout == a.clean_expected_stdout
        a.reset()
