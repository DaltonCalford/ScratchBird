#coding:utf-8

"""
ID:          issue-4525
ISSUE:       4525
TITLE:       An uncommitted select of the pseudo table sec$users blocks new database connections
DESCRIPTION:
JIRA:        CORE-4200
FBTEST:      bugs.core_4200
NOTES:
    [08.12.2023] pzotov
    Removed excessive split: code for FB 3.x and 4.x is identical.
    Initial version of test (.in FBT framework) was created 29 may 2015, so one may to assume
    that bug was fixed at that time. Current QA uses firebird-driver which has no ability to
    make connection to such ancient FB version (3.0 Alpha1 ?). This means that we can not check
    whether this test can reproduce source problem.
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

db = db_factory()

user_srp = user_factory('db', name='tmp$c4200_srp', password='123', plugin='Srp')
user_leg = user_factory('db', name='tmp$c4200_leg', password='123', plugin='Legacy_UserManager')

act = python_act('db')

expected_stdout_2 = """
WHO_AM_I                        TMP$C4200_LEG
AUTH_METHOD                     Leg

WHO_AM_I                        TMP$C4200_SRP
AUTH_METHOD                     Srp
"""

@pytest.mark.version('>=3.0')
def test_2(act: Action, user_srp: User, user_leg: User, capsys):
    act.expected_stdout = expected_stdout_2
    check_sql='select mon$user as who_am_i, left(mon$auth_method,3) as auth_method from mon$attachments'
    custom_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
    #
    with act.db.connect() as con1:
        trn1 = con1.transaction_manager(custom_tpb)
        cur1 = trn1.cursor()
        cur1.execute('select sec$user_name from sec$users').fetchall()
        with act.db.connect(user=user_leg.name, password=user_leg.password) as con2, \
             act.db.connect(user=user_srp.name, password=user_srp.password) as con3:
            trn2 = con2.transaction_manager(custom_tpb)
            cur2 = trn2.cursor()
            act.print_data_list(cur2.execute(check_sql))
            #
            trn3 = con3.transaction_manager(custom_tpb)
            cur3 = trn3.cursor()
            act.print_data_list(cur3.execute(check_sql))
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
