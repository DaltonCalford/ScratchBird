#coding:utf-8

"""
ID:          issue-6508
ISSUE:       6508
TITLE:       Deleting records from MON$ATTACHMENTS using ORDER BY clause doesn't close the corresponding attachments
DESCRIPTION:
JIRA:        CORE-6266
FBTEST:      bugs.core_6266
"""

import pytest
import time
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

act = python_act('db')

expected_stdout = """
    Number of attachments that remains alive: 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    ATT_CNT = 5
    ATT_DELAY = 1
    #
    con_list = []
    for i in range(ATT_CNT):
        if i > 0:
            time.sleep(ATT_DELAY)
            con_list.append(act.db.connect())
    con_admin = con_list[0]
    # This DOES NOT remove all attachments (only 'last' in order of timestamp), but
    # DELETE statement must NOT contain phrase 'mon$attachment_id != current_connection':
    con_admin.execute_immediate('delete from mon$attachments where mon$system_flag is distinct from 1 order by mon$timestamp')
    con_admin.commit()
    #
    cur_admin = con_admin.cursor()
    cur_admin.execute('select mon$attachment_id,mon$user from mon$attachments where mon$system_flag is distinct from 1 and mon$attachment_id != current_connection')
    i = 0
    for r in cur_admin:
        print('STILL ALIVE ATTACHMENT DETECTED: ', r[0], r[1].strip())
        i += 1
    print(f'Number of attachments that remains alive: {i}')
    for con in con_list:
        try:
            con.close()
        except DatabaseError:
            pass
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
