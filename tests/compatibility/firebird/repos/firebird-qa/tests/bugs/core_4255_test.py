#coding:utf-8

"""
ID:          issue-4579
ISSUE:       4579
TITLE:       Parametrized queries using RDB$DB_KEY do not work
DESCRIPTION:
JIRA:        CORE-4255
FBTEST:      bugs.core_4255
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- test by Mark:
    recreate table dbkeytest
    (
        id integer primary key,
        seen boolean default false
    );
    commit;
    insert into dbkeytest (id) values (1);
    insert into dbkeytest (id) values (2);
    insert into dbkeytest (id) values (3);
    insert into dbkeytest (id) values (4);
    commit;

    -- actual test:
    set term ^;
    execute block
    as
        declare thekey char(8);
        declare theid integer;
    begin
        for select id, rdb$db_key from dbkeytest into theid, thekey do
        begin
            execute statement ('update dbkeytest set seen = true where rdb$db_key = ?') (thekey);
        end
    end
    ^
    set term ;^
    commit;

    select * from dbkeytest;

    -- one else test (suggested by Dmitry) in this ticket:
    select 1 x from rdb$database where rdb$db_key = cast((select rdb$db_key from rdb$database) as varchar(8));
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
              ID    SEEN
    ============ =======
               1 <true>
               2 <true>
               3 <true>
               4 <true>

               X
    ============
               1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
