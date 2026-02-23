#coding:utf-8

"""
ID:          issue-5063
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5063
TITLE:       Can not create user with non-ascii (multi-byte) characters in it's name
DESCRIPTION:
NOTES:
[24.11.2021] pcisar
  1. This problem is covered by test for #5048 (CORE-4743; https://github.com/FirebirdSQL/firebird/issues/5048 ) as side effect
  2. For sake of completness, it was reimplemented by simply using
     user_factory fixture.
[09.02.2022] pcisar
  On Windows the act.db.connect() fails with "Your user name and password are not defined."
[08.04.2022] pzotov
  One need to specify utf8filename=True in db_factory() call if we want to establish connection as "non-ascii user".
  Specifying of this parameter in firebird-driver.conf (in the servger section) has no effect.
  Checked on 4.0.1 Release, 5.0.0.467.
  See also:
  email discusion, subject: "firebird-qa [new framework]: unable to make connection as NON-ASCII user, only on Windows (WI-V4.0.1.2692)",
  message from pcisar 08-mar-2022 13:52 ('utf8filename' parameter was added to db_factory()).

JIRA:        CORE-4760 
FBTEST:      bugs.core_4760
"""

import pytest
import platform
from firebird.qa import *

db = db_factory(utf8filename=True)

NON_ASCII_NAME = 'Εὐκλείδης'
non_ascii_user = user_factory('db', name=f'"{NON_ASCII_NAME}"', password='123')

act = python_act('db')

expected_stdout=f"""
    WHOAMI : {NON_ASCII_NAME}
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action, non_ascii_user: User, capsys):
    with act.db.connect(user=non_ascii_user.name, password=non_ascii_user.password) as con:
        cur = con.cursor()
        cur.execute( 'select mon$user as whoami from mon$attachments where mon$attachment_id = current_connection')
        col = cur.description
        for r in cur:
            for i in range(len(col)):
                print(' '.join((col[i][0], ':', r[i])))

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
