#coding:utf-8

"""
ID:          issue-5174
ISSUE:       5174
TITLE:       Minor inconvenience in user management via services API
DESCRIPTION:
JIRA:        CORE-4879
FBTEST:      bugs.core_4879
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = 'We do not expect any error output'
    act.svcmgr(switches=['action_add_user', 'dbname',  str(act.db.db_path),
                       'sec_username', 'TMP$C4879', 'sec_password', '123'])
    # There should be no output at all
    assert act.clean_stderr == ''
    assert act.clean_stdout == ''
    # add_user passed, so remove it
    act.reset()
    act.svcmgr(switches=['action_delete_user', 'dbname',  str(act.db.db_path),
                           'sec_username', 'TMP$C4879'])
