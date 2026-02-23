#coding:utf-8

"""
ID:          issue-2660
ISSUE:       2660
TITLE:       Allow non-SYSDBA users to monitor not only their current attachment but other their attachments as well
DESCRIPTION:
JIRA:        CORE-2233
FBTEST:      bugs.core_2233
"""

import pytest
from firebird.qa import *

substitutions_1 = [('[ \t]+', ' '), ('=', '')]

db = db_factory()

user_mike = user_factory('db', name='tmp$c2233_mike', password='456')
user_adam = user_factory('db', name='tmp$c2233_adam', password='123')
boss = role_factory('db', name='tmp$r2233_boss')
acnt = role_factory('db', name='tmp$r2233_acnt')

act = python_act('db', substitutions=substitutions_1)

expected_stdout = """
    WHO_AM_I 		: TMP$C2233_MIKE
    MON_ATT_USER 	: None
    MON_ATT_ROLE 	: None
    MON_ATT_CNT 	: 0

    WHO_AM_I 		: TMP$C2233_ADAM
    MON_ATT_USER 	: TMP$C2233_ADAM
    MON_ATT_ROLE 	: NONE
    MON_ATT_CNT 	: 1

    WHO_AM_I 		: TMP$C2233_ADAM
    MON_ATT_USER 	: TMP$C2233_ADAM
    MON_ATT_ROLE 	: TMP$R2233_ACNT
    MON_ATT_CNT 	: 1

    WHO_AM_I 		: TMP$C2233_ADAM
    MON_ATT_USER 	: TMP$C2233_ADAM
    MON_ATT_ROLE 	: TMP$R2233_BOSS
    MON_ATT_CNT 	: 1
"""


@pytest.mark.version('>=3')
def test_1(act: Action, user_mike: User, user_adam: User, boss: Role, acnt: Role, capsys):
    act.expected_stdout = expected_stdout
    with act.db.connect() as con:
        c = con.cursor()
        c.execute('grant tmp$r2233_boss to tmp$c2233_adam')
        c.execute('grant tmp$r2233_acnt to tmp$c2233_adam')
        con.commit()
        #
        with act.db.connect(user=user_mike.name, password=user_mike.password) as con0, \
             act.db.connect(user=user_adam.name, password=user_adam.password) as con1, \
             act.db.connect(user=user_adam.name, password=user_adam.password) as con2, \
             act.db.connect(user=user_adam.name, password=user_adam.password, role=boss.name) as con3, \
             act.db.connect(user=user_adam.name, password=user_adam.password, role=acnt.name) as con4:
            #
            chk_sql = """
                select current_user as who_am_i, mon$user mon_att_user, mon$role as mon_att_role, count( mon$user ) as mon_att_cnt
                from rdb$database r
                left join mon$attachments a on a.mon$attachment_id != current_connection
                group by 1,2,3
            """
            #
            cur_mngr = con0.cursor()
            cur_mngr.execute(chk_sql)
            cur_cols = cur_mngr.description
            for r in cur_mngr:
                for i in range(0,len(cur_cols)):
                    print( cur_cols[i][0], ':', r[i] )
            cur_mngr.close()
            #
            cur_boss = con1.cursor()
            cur_boss.execute(chk_sql)
            cur_cols = cur_boss.description
            for r in cur_boss:
                for i in range(0,len(cur_cols)):
                    print( cur_cols[i][0], ':', r[i] )
            cur_boss.close()
            #
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
