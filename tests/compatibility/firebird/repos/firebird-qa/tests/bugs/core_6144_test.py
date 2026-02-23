#coding:utf-8

"""
ID:          issue-6393
ISSUE:       6393
TITLE:       Inconsistent behaviour of the NEW context variable in AFTER UPDATE OR DELETE triggers
DESCRIPTION:
JIRA:        CORE-6144
FBTEST:      bugs.core_6144
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test (id integer not null primary key);
    commit;

    insert into test (id) values (1);
    commit;
    alter table test add v integer default 1 not null;
    commit;
    insert into test (id) values (2);
    commit;
    create exception exc_not_null_in_AD_trigger 'new.v is NOT null in AD trigger ?!';
    commit;

    set term ^;
    create or alter trigger test_null for test after update or delete as
    begin
        if (new.v is not null) then -- new.v should be NULL if the trigger runs after DELETE statement
        begin
            rdb$set_context('USER_SESSION', 'AD_TRIGGER_NEW_V', new.v);
            exception exc_not_null_in_AD_trigger;
        end
    end
    ^
    set term ;^
    commit;

    delete from test where id = 2; -- no errors
    delete from test where id = 1; -- trigger throws exception

    set list on;
    select rdb$get_context('USER_SESSION', 'AD_TRIGGER_NEW_V') as "new_v value in AD trigger:"
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    new_v value in AD trigger:      <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
