#coding:utf-8

"""
ID:          issue-4784
ISSUE:       4784
TITLE:       Duplicate tags for CREATE/ALTER USER not handled correctly
DESCRIPTION:
JIRA:        CORE-4464
FBTEST:      bugs.core_4464
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

db = db_factory()

user_1 = user_factory('db', name='tmp$c4464_1', do_not_create=True, plugin='Srp')
user_2 = user_factory('db', name='tmp$c4464_2', do_not_create=True, plugin='Srp')
user_3 = user_factory('db', name='tmp$c4464_3', do_not_create=True, plugin='Srp')

act = python_act('db')

expected_stdout = """
    USRNAME      TAG_KEY              TAG_VAL                   SEC_PLG
    ============ ==================== ========================= =======
    TMP$C4464_1  AKA                  Ozzy                      Srp
    TMP$C4464_1  GROUPNAME            Black Sabbath             Srp
    TMP$C4464_1  INITNAME             John                      Srp
    TMP$C4464_1  SURNAME              Osbourne                  Srp
    TMP$C4464_2  GROUPNAME            Deep Purple               Srp
    TMP$C4464_2  INITNAME             Ian                       Srp
    TMP$C4464_2  SURNAME              Gillan                    Srp
    TMP$C4464_3  <null>               <null>                    Srp
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42702
    Duplicated user attribute INITNAME

    Statement failed, SQLSTATE = 42702
    Duplicated user attribute INITNAME

    Statement failed, SQLSTATE = 42702
    Duplicated user attribute INITNAME

    Statement failed, SQLSTATE = 42702
    Duplicated user attribute INITNAME
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, user_1: User, user_2: User, user_3: User):

    test_script = f"""
        -- Should fail with:
        --    Statement failed, SQLSTATE = 42702
        --    Duplicated user attribute INITNAME
        -- - because of duplicate specification of added attr. 'initname':
        create user {user_1.name} password '123'
            using plugin Srp
            tags (initname='Ozzy', surname='Osbourne', groupname='Black Sabbath', initname='John')
        ;
        rollback; -- !! --

        -- Should work OK:
        create user {user_1.name} password '123'
            using plugin Srp
            tags (initname='John', surname='Osbourne', groupname='Black Sabbath', aka='Ozzy')
        ;

        -- Should work OK:
        create user {user_2.name} password '456'
            using plugin Srp
            tags (initname='Ian', surname='Gillan', groupname='Deep Purple')
        ;

        create user {user_3.name} password '789'
            using plugin Srp
        ;
        commit;


        -- Should fail with:
        --    Statement failed, SQLSTATE = 42702
        --    Duplicated user attribute INITNAME
        -- - because of duplicate specification of deleted attr. 'initname':
        alter user {user_2.name}
            using plugin Srp
            tags (drop initname, drop surname, drop groupname, drop initname);
        commit;

        -- Should fail with:
        --    Statement failed, SQLSTATE = 42702
        --    Duplicated user attribute INITNAME
        -- - because of duplicate tag to be added: initname
        alter user {user_3.name}
            using plugin Srp
            tags (initname='Ozzy', surname='Osbourne', groupname='Black Sabbath', initname='Foo');
        commit;


        -- Should fail with:
        --    Statement failed, SQLSTATE = 42702
        --    Duplicated user attribute INITNAME
        -- - because of duplicate specification of removed and than added attr. 'initname':
        alter user {user_3.name}
            using plugin Srp
            tags (drop initname, surname='Gillan', groupname='Deep Purple', initname='Ian');
        commit;

        set width usrname 12;
        set width tag_key 20;
        set width tag_val 25;
        set width sec_plg 7;
        select
             u.sec$user_name as usrname
            ,a.sec$key tag_key
            ,a.sec$value as tag_val
            ,sec$plugin sec_plg
        from sec$users u
        left join sec$user_attributes a using( sec$user_name, sec$plugin )
        where u.sec$user_name in ( upper('{user_1.name}'), upper('{user_2.name}'), upper('{user_3.name}') )
        order by 1,2,3;
        commit;
    """

    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.isql(switches=['-q'], input = test_script)
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

