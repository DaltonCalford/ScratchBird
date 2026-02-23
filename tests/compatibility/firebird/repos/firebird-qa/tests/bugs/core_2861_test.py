#coding:utf-8

"""
ID:          issue-3245
ISSUE:       3245
TITLE:       Cannot remove user with dot in login
DESCRIPTION:
  Since 10.01.2016 this test (for 3.0) is based on totally new algorithm with checking ability of
  normal work with randomly generated logins. These logins consists only of punctuation chars and
  for sure will have at least one dot.
  The reason of this replacement was failed results on Classic 3.0 when 'gsec' utility is invoked.
  Code for 2.5 was not changed and is preserved (though it was missed for 2.5 before, but it works OK).

  See http://web.firebirdsql.org/download/prerelease/results/archive/  for builds: 3.0.0.32266 3.0.0.32268

  Correctness of current code was verified by batch scenario, totally about ~1500 iterations was done.
  Several samples of logins that were be checked:
     ,(/;.>_:%$^`.&<|#?=[~\\*}-{@)
     >=[{+%\\.&|~$`(;#._,])}?*@:^!
     }^\\*@.)#>|/;&!=~`]<[,?.-:(%.

  NOTE: currently we EXCLUDE single and double quotes from random login because of CORE-5072.

  This login is handled then by both FBSVCMGR and ISQL utilities:
  1) run FBSVCMGR and:
   1.1) add user
   1.2) modifying some of its attributes (password, firstname etc).
   NOTE! We do *not* run 'fbsvcmgr action_delete_user' because it does not work (at least on build 32268)
   ######################################################################################################
   COMMAND: fbsvcmgr localhost/3333:service_mgr user sysdba password masterkey action_delete_user dbname C:\\MIX\\firebird\\fb30\\security3.fdb sec_username john
   STDERR:  unexpected item in service parameter block, expected isc_spb_sec_username
   (sent letter to Alex, 09-jan-2016 22:34; after getting reply about this issue test can be further changed).
  2) run ISQL and:
  2.1) drop this user that could not be dropped in FBSVCMGR - see previous section.
  2.2) create this login again;
  2.3) modifying some of this login attributes;
  2.4) drop it finally.

  See also:
  #2240 ("Usernames with '.' character"; login 'JOHN.SMITH' is used there).
  #3382 (Error on delete user "ADMIN").
JIRA:        CORE-2861
FBTEST:      bugs.core_2861
"""

import pytest
import string
from random import sample, choice
from firebird.qa import *
from firebird.driver.types import UserInfo

substitutions = [('.* name: .*', 'Name: <name.with.puncts>'),
                   ('.*USER_NAME.*', 'USER_NAME <name.with.puncts>')]

db = db_factory()

act = python_act('db', substitutions=substitutions)

chars = string.punctuation
length = 28
dotted_user = ''.join(sample(chars, length)).replace('"', '.').replace("'", '.')
quoted_user = '"' + dotted_user + '"'

isql_txt = f"""---- {dotted_user}
    set list on;
    --set echo on;
    create or alter view v_sec as select sec$user_name, sec$first_name, sec$middle_name, sec$last_name, sec$admin
    from sec$users
    where upper(sec$user_name)=upper('{dotted_user}');
    commit;

    select 'Try to add user with name: ' || '{dotted_user}' as isql_msg from rdb$database;

    create or alter user {quoted_user} password '123' grant admin role;
    commit;

    select 'Try to display user after adding.' as isql_msg from rdb$database;

    select * from v_sec;
    commit;

    select 'Try to modify user: change password and some attributes.' as isql_msg from rdb$database;

    alter user {quoted_user}
        password 'Zeppelin'
        firstname 'John'
        middlename 'Bonzo The Beast'
        lastname 'Bonham'
        revoke admin role
    ;
    commit;

    select 'Try to display user after modifying.' as isql_msg from rdb$database;
    select * from v_sec;
    commit;

    select 'Try to drop user.' as isql_msg from rdb$database;
    drop user {quoted_user};
    commit;
    select 'All done.' as isql_msg from rdb$database;
"""

expected_stdout = """
ISQL_MSG                        Try to add user with name: <name.with.puncts>
ISQL_MSG                        Try to display user after adding.
SEC$USER_NAME                   <name.with.puncts>
SEC$FIRST_NAME                  <null>
SEC$MIDDLE_NAME                 <null>
SEC$LAST_NAME                   <null>
SEC$ADMIN                       <true>
ISQL_MSG                        Try to modify user: change password and some attributes.
ISQL_MSG                        Try to display user after modifying.
SEC$USER_NAME                   <name.with.puncts>
SEC$FIRST_NAME                  John
SEC$MIDDLE_NAME                 Bonzo The Beast
SEC$LAST_NAME                   Bonham
SEC$ADMIN                       <false>
ISQL_MSG                        Try to drop user.
ISQL_MSG                        All done.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    # Via services
    with act.connect_server() as srv:
        srv.user.add(user_name=quoted_user, password='foobarkey', admin=True)
        user = srv.user.get(user_name=quoted_user)
        assert user == UserInfo(user_name=dotted_user, password=None, admin=True,
                                first_name='', middle_name='', last_name='', user_id=0,
                                group_id=0)
        srv.user.update(user_name=quoted_user, password='BSabbath', first_name='Ozzy',
                        middle_name='The Terrible', last_name='Osbourne', admin=False)
        user = srv.user.get(user_name=quoted_user)
        assert user == UserInfo(user_name=dotted_user, password=None, admin=False,
                                first_name='Ozzy', middle_name='The Terrible', last_name='Osbourne',
                                user_id=0, group_id=0)
        srv.user.delete(user_name=quoted_user)
        assert srv.user.get(user_name=quoted_user) is None
    # Via ISQL
    act.expected_stdout = expected_stdout
    act.isql(switches=[], input=isql_txt)
    assert act.clean_stdout == act.clean_expected_stdout

