#coding:utf-8

"""
ID:          create-database-13
TITLE:       Create database: check that actual FW setting is ON for just created database.
DESCRIPTION:
    Test was requested by dimitr, letter: 15.04.2024 20:32.
    See: https://github.com/FirebirdSQL/firebird/commit/d96d26d0a1cdfd6edcfa8b1bbda8f8da4ec4b5ef
"""
import locale
import pytest
from firebird.qa import *

db = db_factory()
db_temp = db_factory(filename = 'tmp4test.tmp', do_not_create=True)

act = python_act('db', substitutions=[('^((?!(force write)).)*$', ''), ('[\t ]+', ' '),] )

@pytest.mark.version('>=3')
def test_2(act: Action, db_temp: Database,):
    init_script = \
    f"""
        set list on;
        commit;
        create database '{db_temp.dsn}' user {act.db.user} password '{act.db.password}';
        select m.mon$forced_writes as "force write" from mon$database m;
        commit;
    """

    expected_stdout_isql = """
        force write 1
    """

    # Check-1: ensure that mon$database.forced_writes contains 1:
    ##########
    act.isql(switches=['-q'], input=init_script, connect_db = False, credentials = False, combine_output=True)
    act.expected_stdout = expected_stdout_isql
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # Check-2: ensure that 'gstat -h' shows 'force write' in the 'Attributes' line:
    ##########
    act.expected_stdout = """
        Attributes force write
    """
    act.gstat(switches=['-h', db_temp.db_path], connect_db=False, credentials = False, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout


