#coding:utf-8

"""
ID:          issue-4822
ISSUE:       4822
TITLE:       ISQL command SHOW USERS display only me
DESCRIPTION:
NOTES:
[29.07.2016]
  instead of issuing SHOW USERS which has unstable output (can be changed multiple times!)
  it was decided to replace it with SQL query which actually is done by this command.
  This query can be easily found in trace when we run SHOW USERS.
  Also, we limit output with only those users who is enumerated here thus one may do not warry about
  another user logins which could left in securitiN.fdb after some test failed.
[29.03.2018] changed user names, replaced count of SYSDBA attachments with literal 1.
JIRA:        CORE-4503
FBTEST:      bugs.core_4503
"""

import pytest
from firebird.qa import *

db = db_factory()

user_bill = user_factory('db', name='TMP$C4503_BILL', password='123')
user_john = user_factory('db', name='TMP$C4503_JOHN', password='456')
user_mick = user_factory('db', name='TMP$C4503_MICK', password='789')
user_boss = user_factory('db', name='TMP$C4503_BOSS', password='000')

act = python_act('db')

script = """
set list on;
-- "SHOW USERS" command actually runs following query:
select
    case
        when coalesce(mon$user, sec$user_name) = current_user
            then '#'
        when sec$user_name is distinct from null
            then ' '
        else '-'
    end is_current_user
    ,coalesce(m.mon$user, u.sec$user_name) user_name
    ,iif( m.mon$user = upper('SYSDBA'), 1, count(m.mon$user) ) keeps_attachments
from mon$attachments m
full join sec$users u on m.mon$user = u.sec$user_name
where
    coalesce(mon$system_flag, 0) = 0
    and coalesce(m.mon$user, u.sec$user_name) in ( upper('TMP$C4503_BILL'), upper('TMP$C4503_BOSS'), upper('TMP$C4503_JOHN'), upper('TMP$C4503_MICK'), upper('SYSDBA') )
group by mon$user, sec$user_name
order by coalesce(mon$user, sec$user_name);
commit;
"""

expected_stdout_1 = """
     IS_CURRENT_USER                 #
     USER_NAME                       SYSDBA
     KEEPS_ATTACHMENTS               1

     IS_CURRENT_USER
     USER_NAME                       TMP$C4503_BILL
     KEEPS_ATTACHMENTS               5

     IS_CURRENT_USER
     USER_NAME                       TMP$C4503_BOSS
     KEEPS_ATTACHMENTS               0

     IS_CURRENT_USER
     USER_NAME                       TMP$C4503_JOHN
     KEEPS_ATTACHMENTS               4

     IS_CURRENT_USER
     USER_NAME                       TMP$C4503_MICK
     KEEPS_ATTACHMENTS               1
"""


@pytest.mark.version('>=3.0')
def test_1(act: Action, user_bill: User, user_john: User, user_mick: User, user_boss: User):
    with act.db.connect() as con_0a, act.db.connect(), \
         act.db.connect(user='TMP$C4503_BILL', password='123'), \
         act.db.connect(user='TMP$C4503_BILL', password='123'), \
         act.db.connect(user='TMP$C4503_BILL', password='123'), \
         act.db.connect(user='TMP$C4503_BILL', password='123'), \
         act.db.connect(user='TMP$C4503_BILL', password='123'), \
         act.db.connect(user='TMP$C4503_JOHN', password='456'), \
         act.db.connect(user='TMP$C4503_JOHN', password='456'), \
         act.db.connect(user='TMP$C4503_JOHN', password='456'), \
         act.db.connect(user='TMP$C4503_JOHN', password='456'), \
         act.db.connect(user='TMP$C4503_MICK', password='789'):
        #
        act.expected_stdout = expected_stdout_1
        act.isql(switches=['-q'], input=script)
        assert act.clean_stdout == act.clean_expected_stdout
