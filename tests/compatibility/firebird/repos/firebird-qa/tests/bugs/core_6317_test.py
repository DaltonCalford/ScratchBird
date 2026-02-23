#coding:utf-8

"""
ID:          issue-6558
ISSUE:       6558
TITLE:       Server is crashing on long GRANT statement
DESCRIPTION:
JIRA:        CORE-6317
FBTEST:      bugs.core_6317
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set wng off;
    create or alter user tmp$c6317 password '123';
    revoke all on all from tmp$c6317;
    commit;

    recreate table test (id integer);
    insert into test(id) values(1);
    commit;

    grant select, select, select, select, select, select, select, select, select, select, select, select, select, select, select, select on test to tmp$c6317;
    commit;

    set list on;
    select
         rdb$user                        -- tmp$c6317
        ,rdb$relation_name               -- test
        ,rdb$privilege                   -- S
        ,rdb$grant_option                -- 0
        ,rdb$field_name                  -- <null>
        ,rdb$object_type                 -- 0
    from rdb$user_privileges p
    where upper(p.rdb$relation_name) = upper('test') and rdb$user = upper('tmp$c6317')
    order by rdb$privilege
    ;
    commit;

    connect '$(DSN)' user tmp$c6317 password '123';
    select * from test;
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c6317;
    commit;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    RDB$USER                        TMP$C6317
    RDB$RELATION_NAME               TEST
    RDB$PRIVILEGE                   S
    RDB$GRANT_OPTION                0
    RDB$FIELD_NAME                  <null>
    RDB$OBJECT_TYPE                 0

    ID                              1
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
