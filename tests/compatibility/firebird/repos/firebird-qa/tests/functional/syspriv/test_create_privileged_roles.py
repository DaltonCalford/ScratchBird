#coding:utf-8

"""
ID:          syspriv.create-privileged-roles
TITLE:       Check ability of non-sysdba user to CREATE privileged role (but NOT use it)
DESCRIPTION:
FBTEST:      functional.syspriv.create_privileged_roles
"""

import pytest
from firebird.qa import *

db = db_factory()
test_user = user_factory('db', name='u01', do_not_create=True)
role_create = role_factory('db', name='role_for_CREATE_PRIVILEGED_ROLES', do_not_create=True)
role_granted = role_factory('db', name='role_for_USE_GRANTED_BY_CLAUSE', do_not_create=True)

test_script = """
    set wng off;
    set bail on;
    set list on;

    create or alter user u01 password '123' revoke admin role;
    revoke all on all from u01;
    grant create role to u01;
    commit;
/*
    set term ^;
    execute block as
    begin
      begin
        execute statement 'drop role role_for_CREATE_PRIVILEGED_ROLES';
        when any do begin end
      end
      begin
        execute statement 'drop role role_for_USE_GRANTED_BY_CLAUSE';
        when any do begin end
      end
    end^
    set term ;^
    commit;
*/
    create role role_for_CREATE_PRIVILEGED_ROLES set system privileges to CREATE_PRIVILEGED_ROLES;
    commit;
    grant default role_for_CREATE_PRIVILEGED_ROLES to user u01;
    commit;

    connect '$(DSN)' user u01 password '123';

    -- Here we check that U01 can CREATE privileged role but this is the ONLY what he can do,
    -- i.e. he is NOT granted with this role!
    create role role_for_USE_GRANTED_BY_CLAUSE SET SYSTEM PRIVILEGES TO USE_GRANTED_BY_CLAUSE;
    commit;

    select current_user as who_am_i,r.rdb$role_name,rdb$role_in_use(r.rdb$role_name),r.rdb$system_privileges
    from mon$database m cross join rdb$roles r;

    commit;

    --connect '$(DSN)' user sysdba password 'masterkey';
    --drop user u01;
    --commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
     WHO_AM_I                        U01
     RDB$ROLE_NAME                   RDB$ADMIN
     RDB$ROLE_IN_USE                 <false>
     RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF

     WHO_AM_I                        U01
     RDB$ROLE_NAME                   ROLE_FOR_CREATE_PRIVILEGED_ROLES
     RDB$ROLE_IN_USE                 <true>
     RDB$SYSTEM_PRIVILEGES           0000800000000000

     WHO_AM_I                        U01
     RDB$ROLE_NAME                   ROLE_FOR_USE_GRANTED_BY_CLAUSE
     RDB$ROLE_IN_USE                 <false>
     RDB$SYSTEM_PRIVILEGES           0000100000000000
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, test_user, role_create, role_granted):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
