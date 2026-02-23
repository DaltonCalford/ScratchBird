#coding:utf-8

"""
ID:          issue-2067
ISSUE:       2067
TITLE:       Non-privileged monitoring reports wrong attachment data
DESCRIPTION:
  When non-SYSDBA user selects from MON$ATTACHMENTS and other attachments are active at this point,
  the resulting rowset refers to a wrong attachment (the one with minimal ID) instead of the current attachment.
JIRA:        CORE-1642
FBTEST:      bugs.core_1642
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter view v_my_attach as
    select current_user as who_am_i, iif(current_connection - mon$attachment_id = 0, 'OK.', 'BAD') as my_attach_id
    from mon$attachments;
    commit;

    grant select on v_my_attach to public;
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
    TMP$C1642_ALAN OK.
    TMP$C1642_JOHN OK.
    TMP$C1642_MICK OK.
"""

user_1 = user_factory('db', name='tmp$c1642_alan', password='123')
user_2 = user_factory('db', name='tmp$c1642_john', password = '456')
user_3 = user_factory('db', name='tmp$c1642_mick', password = '789')

@pytest.mark.version('>=2.5')
def test_1(act: Action, user_1: User, user_2: User, user_3: User, capsys):
    act.expected_stdout = expected_stdout
    for user in [user_1, user_2, user_3]:
        with act.db.connect(user=user.name, password=user.password) as con:
            c = con.cursor()
            c.execute('select who_am_i, my_attach_id from v_my_attach')
            for row in c:
                print(row[0], row[1])
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout





