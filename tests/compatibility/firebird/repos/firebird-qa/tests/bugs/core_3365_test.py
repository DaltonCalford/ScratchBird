#coding:utf-8

"""
ID:          issue-3731
ISSUE:       3731
TITLE:       Extend syntax for ALTER USER CURRENT_USER
DESCRIPTION:
JIRA:        CORE-3365
FBTEST:      bugs.core_3365
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set count on;

    set width usrname 10;
    set width firstname 10;
    set width sec_plugin 20;
    set width sec_attr_key 20;
    set width sec_attr_val 20;
    set width sec_plugin 20;

    recreate view v_usr_info as
    select
         su.sec$user_name as usrname
        ,su.sec$first_name as firstname
        ,su.sec$plugin as sec_plugin
        ,sa.sec$key as sec_attr_key
        ,sa.sec$value as sec_attr_val
    from sec$users su left
    join sec$user_attributes sa using(sec$user_name, sec$plugin)
    where su.sec$user_name = upper('tmp$c3365');
    commit;

    grant select on v_usr_info to public;
    commit;

    create user tmp$c3365 password 'Ir0nM@n' firstname 'John'
        using plugin Srp
        tags (initname='Ozzy', surname='Osbourne', groupname='Black Sabbath', birthday = '03.12.1948')
    ;
    commit;

    select 'before altering' as msg, v.* from v_usr_info v;
    commit;

    connect '$(DSN)' user tmp$c3365 password 'Ir0nM@n';

    alter current user
        set password 'H1ghWaySt@r' firstname 'Ian'
        using plugin Srp
        tags (initname='Ian', surname='Gillan', groupname='Deep Purple', drop birthday)
    ;
    commit;

    connect '$(DSN)' user tmp$c3365 password 'H1ghWaySt@r';
    commit;

    select 'after altering' as msg, v.* from v_usr_info v;
    commit;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('=', '')])

test_user = user_factory('db', name='tmp$c3365', do_not_create=True)

expected_stdout = """
    MSG             USRNAME    FIRSTNAME  SEC_PLUGIN           SEC_ATTR_KEY         SEC_ATTR_VAL
    =============== ========== ========== ==================== ==================== ====================
    before altering TMP$C3365  John       Srp                  BIRTHDAY             03.12.1948
    before altering TMP$C3365  John       Srp                  GROUPNAME            Black Sabbath
    before altering TMP$C3365  John       Srp                  INITNAME             Ozzy
    before altering TMP$C3365  John       Srp                  SURNAME              Osbourne
    Records affected: 4

    MSG            USRNAME    FIRSTNAME  SEC_PLUGIN           SEC_ATTR_KEY         SEC_ATTR_VAL
    ============== ========== ========== ==================== ==================== ====================
    after altering TMP$C3365  Ian        Srp                  GROUPNAME            Deep Purple
    after altering TMP$C3365  Ian        Srp                  INITNAME             Ian
    after altering TMP$C3365  Ian        Srp                  SURNAME              Gillan
    Records affected: 3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, test_user):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

