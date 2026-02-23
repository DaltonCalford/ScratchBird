#coding:utf-8

"""
ID:          issue-4937
ISSUE:       4937
TITLE:       Regression: Trigger with UPDATE OR INSERT statement and IIF() not working as expected
DESCRIPTION:
JIRA:        CORE-4622
FBTEST:      bugs.core_4622
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
    begin
        execute statement 'drop trigger v_test_bu';
    when any do begin end
    end
    ^
    set term ;^
    commit;

    create or alter view v_test as select 1 id from rdb$database;
    commit;
    -------------------------------------------------------------
    recreate table test (
        id integer not null,
        a integer,
        b integer,
        constraint pk_test primary key (id)
    );
    commit;
    insert into test values(1, 100, 200);
    commit;

    create or alter view v_test as
    select id, a, b from test;
    commit;

    set term ^;
    create or alter trigger v_test_bu for v_test
    active before update position 0
    as
    begin
        -- Confirmed on WI-T3.0.0.31374 Firebird 3.0 Beta 1:
        -- Statement failed, SQLSTATE = HY000
        -- invalid request BLR at offset 51
        -- -undefined variable number
        update or insert into test ( id , a , b)
        values ( iif( mod( new.id,2)=0, -new.id, new.id ), new.a, new.b)
        matching( id );
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
