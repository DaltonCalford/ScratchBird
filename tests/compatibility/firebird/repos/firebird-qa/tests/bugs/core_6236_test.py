#coding:utf-8

"""
ID:          issue-6480
ISSUE:       6480
TITLE:       RDB$TIME_ZONE_UTIL package has wrong privilege for PUBLIC
DESCRIPTION:
  Thanks Adriano for suggestion about this test implementation.
  We create non-privileged user ('tmp$c6236') and do connect of him
  with trying to use package function rdb$time_zone_util.database_version().
  It must pass without any errors (result of call no matter).

  Confirmed exception on 4.0.0.1714: no permission for EXECUTE access to PACKAGE RDB$TIME_ZONE_UTIL
  Checked on 4.0.0.1740 SS: 1.400s - works fine.
  ::: NB :::
  Command 'SHOW GRANTS' does not display privileges on system objects thus we do not use it here.
JIRA:        CORE-6236
FBTEST:      bugs.core_6236
"""

import pytest
from firebird.qa import *

db = db_factory()

test_user = user_factory('db', name='tmp$c6236', password='123')

act = python_act('db')

expected_stdout = """
    DB_VERS_DEFINED                 True
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, test_user: User, capsys):
    with act.db.connect(user=test_user.name, password=test_user.password) as con:
        c = con.cursor()
        c.execute('select rdb$time_zone_util.database_version() is not null as db_vers_defined from rdb$database')
        act.print_data_list(c)
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
