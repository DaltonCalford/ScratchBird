#coding:utf-8

"""
ID:          868145d012
ISSUE:       https://www.sqlite.org/src/tktview/868145d012
TITLE:       Assertion fault on double LEFT JOIN (after added support for transitive constraints)
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    -- set echo on;
    create table tmain (
        id integer primary key,
        a_uid varchar(36)
    );

    create table tdetl_1 (
        id integer primary key,
        uid varchar(36),
        t integer
    );

    create table tdetl_2 (
        id integer primary key,
        uid varchar(36),
        t integer
    );
    insert into tmain(id, a_uid) values(1, '1234');
    insert into tdetl_1(id, uid, t) values(2, '1234',  100);
    insert into tdetl_2(id, uid, t) values(3, '1234', 100);

    set count on;
    select distinct m.*, d1.*, d2.*
    from
        tmain m
        left join tdetl_1 d1 on d1.uid = '1234'
        left join tdetl_2 d2 on m.a_uid = d2.uid
    where
        d1.t = d2.t
    ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 1
    A_UID 1234

    ID 2
    UID 1234
    T 100

    ID 3
    UID 1234
    T 100

    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
