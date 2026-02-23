#coding:utf-8

"""
ID:          issue-1570
ISSUE:       1570
TITLE:       Every user can view server log using services API
DESCRIPTION:
JIRA:        CORE-1148
FBTEST:      bugs.core_1148
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

substitutions = [('ENGINE_VERSION .*', 'ENGINE_VERSION'),
                 ('STDERR: UNABLE TO PERFORM OPERATION.*', 'STDERR: UNABLE TO PERFORM OPERATION'),
                 ('STDERR: -YOU MUST HAVE SYSDBA RIGHTS AT THIS SERVER*', '')]

db = db_factory()

act = python_act('db', substitutions=substitutions)

tmp_user = user_factory('db', name='TMP$C1148', password='QweRtyUi')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    if act.is_version('<4'):
        if act.platform == 'Windows':
            pattern = 'Unable to perform operation'
        else:
            pattern = 'You must be either SYSDBA or owner of the database'
    else:
        pattern = 'You must have SYSDBA rights at this server'
    with act.connect_server(user=tmp_user.name, password=tmp_user.password) as srv:
        with pytest.raises(DatabaseError, match=pattern):
            srv.info.get_log()

