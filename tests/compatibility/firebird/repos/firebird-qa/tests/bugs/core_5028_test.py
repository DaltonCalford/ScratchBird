#coding:utf-8

"""
ID:          issue-5315
ISSUE:       5315
TITLE:       Report the remote port number in MON$ATTACHMENTS
DESCRIPTION:
JIRA:        CORE-5028
FBTEST:      bugs.core_5028
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    OK
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        c = con.cursor()
        cmd = """
        select iif(port > 0, 'OK', 'BAD') as port_value
        from (
            select cast(substring(mon$remote_address from 1 + position('/' in mon$remote_address)) as int) as port
            from mon$attachments
            where mon$attachment_id = current_connection)
        """
        for row in c.execute(cmd):
            print(row[0])
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout


