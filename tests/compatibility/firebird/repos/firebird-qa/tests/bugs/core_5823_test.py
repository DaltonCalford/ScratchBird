#coding:utf-8

"""
ID:          issue-6084
ISSUE:       6084
TITLE:       No permission for SELECT access to blob field in stored procedure
DESCRIPTION:
JIRA:        CORE-5823
FBTEST:      bugs.core_5823
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp$c5823', password='123')
tmp_role = role_factory('db', name='blob_viewer')

test_script = """
    create or alter procedure test_proc (id integer) as begin end;
    commit;

    recreate table test (
        id integer,
        blb blob
    );
    commit;

    insert into test (id, blb) values (1, 'blob1');
    commit;

    set term ^;
    create or alter procedure test_proc (id integer) returns (blb blob) as
    begin
        for
            select blb from test where id = :id
        into blb
            do suspend;
    end
    ^
    set term ;^
    commit;

    grant select on test to procedure test_proc;
    grant execute on procedure test_proc to blob_viewer;
    grant blob_viewer to tmp$c5823;
    commit;

    connect '$(DSN)' user 'tmp$c5823' password '123' role 'blob_viewer';

    set list on;
    set blob on;
    select mon$user, mon$role from mon$attachments where mon$attachment_id = current_connection;

    select blb as blob_field_id from test_proc(1);
    commit;
"""

act = isql_act('db', test_script, substitutions=[('BLOB_FIELD_ID.*', '')])

expected_stdout = """

    MON$USER                        TMP$C5823
    MON$ROLE                        BLOB_VIEWER
    BLOB_FIELD_ID                   80:0
    blob1
"""

@pytest.mark.version('>=3.0.5')
def test_1(act: Action, tmp_user: User, tmp_role: Role):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
