#coding:utf-8

"""
ID:          issue-3869
ISSUE:       3869
JIRA:        CORE-3511
FBTEST:      bugs.core_3511
TITLE:       Unquoted role names with non-ASCII characters passed in DPB are upper-cased wrongly
DESCRIPTION:
  Test creates role with non-ascii characters and grants it to common user.
  Then we try to connect to database using this role and specify it WITHOUT double quotes,
  with checking that mon$role data not in uppercase.

NOTES:
  [28.05.2022] pzotov
  1. On Windows ISQL utility can *not* be used (in common case) for passing non-ascii data in user or role
  because it allows to use only data encoded in the same codepage as OS system codepage (e.g. 1251 for Russian etc).
  (see letter from dimitr, 12-mar-2016, 19:14).
  Because of this, connection using non-ascii user/role is established only via Python driver API.
  2. firebird.conf must contain 'Srp' in UserManager and AuthClient params for this test can work. Otherwise we get:
  "firebird.driver.types.DatabaseError: Error occurred during login, please check server firebird.log for details"
  
  Checked on 5.0.0.501, 4.0.1.2692, 3.0.8.33535 - both on Linux (CentOS 7.9.2009) and Windows 10 (localized, Russian)
"""

import pytest
from firebird.qa import *
import locale

db = db_factory(utf8filename=True, charset='utf8') # !!see also: core_4743_test.py

tmp_user = user_factory('db', name='"Björn Pålsson"', password='123', plugin='Srp')

#tmp_role1 = role_factory('db', name = '"MaîtreŀłØÐø"')
tmp_role1 = role_factory('db', name = '"Maître"')
tmp_role2 = role_factory('db', name = '"Groß"')

act = python_act('db')

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User, tmp_role1: Role, tmp_role2: Role, capsys):
    init_script = f"""
        set wng off;
        set bail on;

        create or alter view v_whoami as
        select current_user as who_am_i, mon$role as mon_role, iif( mon$role = upper(mon$role), 'WRONG: IN UPPERCASE!', 'Passed') as role_chk
        from mon$attachments
        where mon$attachment_id = current_connection;
        commit;

        grant select on v_whoami to public;
        grant {tmp_role1.name} to {tmp_user.name};
        grant {tmp_role2.name} to {tmp_user.name};

        commit;
    """
    act.isql(switches=['-q'], input = init_script, combine_output=True, charset='utf8')

    expected_stdout = f"""
        WHO_AM_I : {tmp_user.name}
        MON_ROLE : {tmp_role1.name}
        ROLE_CHK : Passed

        WHO_AM_I : {tmp_user.name}
        MON_ROLE : {tmp_role2.name}
        ROLE_CHK : Passed
    """.replace('"','')

    for r in (tmp_role1,tmp_role2):
        with act.db.connect(user = tmp_user.name, password = tmp_user.password, role = r.name.replace('"','')) as con:
            cur = con.cursor()
            cur.execute('select * from v_whoami')
            col = cur.description
            for r in cur:
                for i in range(len(col)):
                    print(' '.join((col[i][0], ':', r[i])))

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

