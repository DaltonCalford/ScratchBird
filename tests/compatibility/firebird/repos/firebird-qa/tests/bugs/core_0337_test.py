#coding:utf-8

"""
ID:          issue-674
ISSUE:       674
TITLE:       ISQL and database dialect
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_25.script

  When ISQL disconnects from database (either by dropping it or by trying to connect to
  non-existent database) is still remembers its sql dialect, which can lead to some
  inappropriate warning messages.

  Issue in original script: bug #910430 ISQL and database dialect
  Found in FB tracker as: http://tracker.firebirdsql.org/browse/CORE-337
  Fixed in 2.0 Beta 1
JIRA:        CORE-337
FBTEST:      bugs.core_0337
"""

import pytest
from firebird.qa import *

db = db_factory()

expected_stdout = """
    SHOW SQL DIALECT;
    Client SQL dialect has not been set and no database has been connected yet.

    SET SQL DIALECT 1;
    SHOW SQL DIALECT;
    Client SQL dialect is set to: 1. No database has been connected.

    SET SQL DIALECT 3;
    CREATE DATABASE 'LOCALHOST:C:\\FBTESTING\\QA\\FBT-REPO\\TMP2\\TMP_0337.FDB' USER 'SYSDBA' PASSWORD 'MASTERKEY';

    SHOW SQL DIALECT;
    Client SQL dialect is set to: 3 and database SQL dialect is: 3

    DROP DATABASE;

    SHOW DATABASE;

    SHOW SQL DIALECT;
    Client SQL dialect is set to: 3. No database has been connected.

    SET SQL DIALECT 1;
"""

expected_stderr = """
Use CONNECT or CREATE DATABASE to specify a database
Use CONNECT or CREATE DATABASE to specify a database
Command error: SHOW DATABASE
"""

act = isql_act('db', "", substitutions=[('[ \t]+', ' '), ('CREATE DATABASE.*', 'CREATE DATABASE')])

@pytest.mark.version('>=2.5')
def test_1(act: Action):
    act.db.drop()
    act.script = f"""
    SET ECHO ON;
    SHOW SQL DIALECT;
    SET SQL DIALECT 1;
    SHOW SQL DIALECT;
    SET SQL DIALECT 3;
    CREATE DATABASE '{act.db.dsn}' USER '{act.db.user}' PASSWORD '{act.db.password}';
    SHOW SQL DIALECT;
    DROP DATABASE;
    SHOW DATABASE;
    SHOW SQL DIALECT;
    SET SQL DIALECT 1;
"""
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute(do_not_connect=True)
    assert (act.clean_stdout == act.clean_expected_stdout and act.clean_stderr == act.clean_expected_stderr)
    act.db.create() # For cleanup



