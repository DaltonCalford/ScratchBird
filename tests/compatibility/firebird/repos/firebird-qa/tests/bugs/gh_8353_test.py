#coding:utf-8

"""
ID:          issue-8353
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8353
TITLE:       Report unique usernames for isc_info_user_names
DESCRIPTION:
    Test uses DbInfoCode.USER_NAMES property to obtain AGGREGATED info for every connected user (instead of getting list).
    This info looks like:
        <USERNAME> : <N_COUNT>
    - where:
        <USERNAME> = name of connected user
        <N_COUNT>  = number of attachments created by <USERNAME>, total for BOTH auth plugins: Srp + Legacy.

NOTES:
    [19.12.2024] pzotov
    Confirmed ticket issue on 6.0.0.552: N_COUNT > 1 are shown for both SYSDBA and non-dba users when they make more than one attachment.
    Checked on 6.0.0.553-7ebb66b, 5.0.2.1580-7961de2, 4.0.6.3172-4119f625: every user is specified only once, i.e. N_COUNT = 1

    [21.12.2024] pzotov
    Added pytest.skip() directive if ServerMode is not Super because there is no way to get info about other users for NON-dba.
    See letter from Vlad, 21.12.2024 13:07.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError, DbInfoCode

db = db_factory()
act = python_act('db', substitutions = [('[ \t]+', ' ')])

N_CONNECTIONS = 3
TMP_USER_NAME = 'TMP$8353'

tmp_user_leg = user_factory('db', name = TMP_USER_NAME, password = '123', plugin = 'Legacy_UserManager')
tmp_user_srp = user_factory('db', name = TMP_USER_NAME, password = '456', plugin = 'Srp')

# set width mon_user 16;
# set width auth_method 20;
# select mon$attachment_id, trim(mon$user) as mon_user, mon$auth_method as auth_method, count(*)over(partition by mon$user), count(*)over(partition by mon$user, mon$auth_method) from mon$attachments;

#-----------------------------------------------------------------------

@pytest.mark.version('>=4.0.6')
def test_1(act: Action, tmp_user_leg: User, tmp_user_srp: User, capsys):

    if act.vars['server-arch'] != 'SuperServer':
        pytest.skip("Can not be checked on CS/SC.")

    try:
        with act.db.connect() as con1, \
             act.db.connect() as con2:
            conn_lst = []
            for i in range(N_CONNECTIONS):
                for u in (tmp_user_leg, tmp_user_srp):
                    conn_lst.append( act.db.connect(user = u.name, password = u.password) )

            for k,v in sorted(con1.info.get_info(DbInfoCode.USER_NAMES).items()):
                print(k,':',v)

            for c in conn_lst:
                c.close()

    except DatabaseError as e:
        print(e.__str__())

    act.expected_stdout = f"""
        {act.db.user} : 1
        {TMP_USER_NAME} : 1
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
