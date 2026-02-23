#coding:utf-8

"""
ID:          issue-6838
ISSUE:       6838
TITLE:       Deleting multiple rows from a view with triggers may cause triggers to fire just once
DESCRIPTION:
FBTEST:      bugs.gh_6838
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t (id int);
    create view v as select id from t;
    create table log (txt varchar(10));

    set term ^;
    create trigger trg for v before update or delete as
    begin
        insert into log values ( iif(deleting, 'deleted', 'updated') );
    end
    ^
    set term ;^

    insert into t values (1);
    insert into t values (2);
    commit;

    delete from v;
    set count on;
    select * from log;
    set count off;
    rollback;

    merge into v t
    using v s on s.id = t.id
    when matched then delete;

    set count on;
    select * from log;
    set count off;
    rollback;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TXT                             deleted
    TXT                             deleted
    Records affected: 2

    TXT                             deleted
    TXT                             deleted
    Records affected: 2
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
