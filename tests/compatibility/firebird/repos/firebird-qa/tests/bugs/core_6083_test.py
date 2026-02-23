#coding:utf-8

"""
ID:          issue-6333
ISSUE:       6333
TITLE:       USING PLUGIN clause does not work in RECREATE USER
DESCRIPTION:
JIRA:        CORE-6083
FBTEST:      bugs.core_6083
"""

import pytest
from firebird.qa import *

db = db_factory()

# fixture used to cleanup only
tmp_user = user_factory('db', name='tmp$c6083', plugin='Srp', do_not_create=True)

test_script = """
    -- Following code should NOT raise any output: neither in STDOUT nor in STDERR.
    set bail on;
    set term ^;
    execute block as
    begin
       begin
           -- Here we 'silently drop' user if it remained after previous (failed) run of this test.
           -- Exception about non-existent user will be suppressed:
           execute statement( 'drop user tmp$c6083 using plugin Srp' ) with autonomous transaction;
       when any do
           begin
           end
       end
    end
    ^
    set term ;^
    commit;

    recreate user tmp$c6083 password '123' using plugin Srp;
    recreate user tmp$c6083 password '456' using plugin Srp; -- THIS (second) statement raised error before ticket was fixed.
    commit;

    connect '$(DSN)' user tmp$c6083 password '456'; -- here we want to be sure that user was created SUCCESSFULLY.
    commit;
/*
    connect '$(DSN)' user sysdba password 'masterkey';

    drop user tmp$c6083 using plugin Srp;
    commit; */
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user):
    act.execute()
