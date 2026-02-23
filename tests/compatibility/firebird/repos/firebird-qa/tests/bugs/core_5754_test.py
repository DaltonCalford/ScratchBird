#coding:utf-8

"""
ID:          issue-6017
ISSUE:       6017
TITLE:       ALTER TRIGGER check privilege for alter database instead of table
DESCRIPTION:
JIRA:        CORE-5754
FBTEST:      bugs.core_5754
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;

    create or alter user tmp$c5754 password '123';
    commit;

    recreate table test(id int);
    recreate sequence g;
    commit;

    -- GRANT ALTER ANY <OBJECT> TO [USER | ROLE] <user/role name> [WITH GRANT OPTION];
    -- DDL operations for managing triggers and indices re-use table privileges.
    grant alter any table to tmp$c5754;
    commit;

    set term ^;
    create or alter trigger test_bi for test active before insert position 0 as
    begin
       new.id = coalesce(new.id, gen_id(g, 1) );
    end
    ^
    set term ;^
    commit;

    connect '$(DSN)' user tmp$c5754 password '123';

    set term  ^;

    -- Following attempt to alter trigger will fail on 4.0.0.890 with message:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER TRIGGER TEST_BI failed
    -- -no permission for ALTER access to DATABASE
    alter trigger test_bi as
    begin
       -- this trigger was updated by tmp$c5754
       if ( new.id is null ) then
           new.id = gen_id(g, 1);
    end
    ^
    set term ;^
    commit;

    commit;
    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c5754;
    commit;

    set list on;

    select 1 as result
    from rdb$triggers
    where rdb$trigger_name = upper('test_bi')
    and rdb$trigger_source containing 'tmp$c5754'
    ;

"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT                          1
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
