#coding:utf-8

"""
ID:          issue-2274
ISSUE:       2274
TITLE:       Some standard calls show server installation directory to regular users
DESCRIPTION:
JIRA:        CORE-1845
FBTEST:      bugs.core_1845
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

act = python_act('db')

tmp_user = user_factory('db', name='TMP$C1845', password='QweRtyUi')

@pytest.mark.version('>=2.5')
def test_1(act: Action, tmp_user: User):
    with act.connect_server(user=tmp_user.name, password=tmp_user.password) as srv:
        with pytest.raises(DatabaseError, match='.*requires SYSDBA permissions.*'):
            print(srv.info.security_database)
        with pytest.raises(DatabaseError, match='.*requires SYSDBA permissions.*'):
            print(srv.info.home_directory)
        with pytest.raises(DatabaseError, match='.*requires SYSDBA permissions.*'):
            print(srv.info.lock_directory)
        with pytest.raises(DatabaseError, match='.*requires SYSDBA permissions.*'):
            print(srv.info.message_directory)
        with pytest.raises(DatabaseError, match='.*requires SYSDBA permissions.*'):
            print(srv.info.attached_databases)

