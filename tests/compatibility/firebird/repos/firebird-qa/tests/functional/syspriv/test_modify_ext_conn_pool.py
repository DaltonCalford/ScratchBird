#coding:utf-8

"""
ID:          syspriv.modify-ext-conn-pool
TITLE:       Check ability to manage external connections pool
DESCRIPTION:
  Verify ability to issue ALTER EXTERNAL CONNECTIONS POOL <...> by non-sysdba user.
FBTEST:      functional.syspriv.modify_ext_conn_pool
"""

import pytest
from firebird.qa import *

db = db_factory()
test_user = user_factory('db', name='john_smith_extpool_manager', do_not_create=True)
test_role = role_factory('db', name='tmp_role_for_change_extpool', do_not_create=True)

test_script = """
    set wng off;
    set list on;

    create or alter
         user john_smith_extpool_manager
         password '123'
    ;
/*
    set term ^;
    execute block as
    begin
        execute statement 'drop role tmp_role_for_change_extpool';
        when any do begin end
    end^
    set term ;^
*/
    create role tmp_role_for_change_extpool set system privileges to MODIFY_EXT_CONN_POOL;
    commit;

    grant default tmp_role_for_change_extpool to user john_smith_extpool_manager;
    commit;

    connect '$(DSN)' user john_smith_extpool_manager password '123';

    alter external connections pool set size 345;
    alter external connections pool set lifetime 789 second;
    commit;

    select
        cast(rdb$get_context('SYSTEM', 'EXT_CONN_POOL_SIZE') as int) as pool_size,
        cast(rdb$get_context('SYSTEM', 'EXT_CONN_POOL_LIFETIME') as int) as pool_lifetime
    from rdb$database;
    rollback;

    -- connect '$(DSN)' user sysdba password 'masterkey';
    -- drop user john_smith_extpool_manager;
    -- drop role tmp_role_for_change_extpool;
    -- commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    POOL_SIZE                       345
    POOL_LIFETIME                   789
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, test_user, test_role):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
