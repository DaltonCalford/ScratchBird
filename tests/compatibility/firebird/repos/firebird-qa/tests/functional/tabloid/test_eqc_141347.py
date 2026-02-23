#coding:utf-8

"""
ID:          tabloid.eqc-141347
TITLE:       Check correctness of LEFT JOIN result when right source has two FK and one of
  fields from FK present both in ON and WHERE clauses.
DESCRIPTION:
FBTEST:      functional.tabloid.eqc_141347
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t2 (
        id     int primary key,
        pid0  int,
        pid1  int
    );
    recreate table t1 (id int primary key);
    recreate table t0 (id int primary key);

    alter table t2
        add constraint fk_t0 foreign key (pid0) references t0
        ,add constraint fk_t1 foreign key (pid1) references t1
    ;
    commit;

    insert into t0 (id) values (1);

    insert into t1 (id) values (1);
    insert into t1 (id) values (2);
    insert into t1 (id) values (3);
    insert into t1 (id) values (4);

    insert into t2 (id, pid1, pid0) values (1, 1, 1);
    insert into t2 (id, pid1, pid0) values (2, 4, 1);
    commit;

    select a.id, b.pid1
    from t1 a
    left join t2 b on ( a.id = b.pid1 ) and ( b.pid0 = 1 )
    where (b.pid1 is null);
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              2
    PID1                            <null>

    ID                              3
    PID1                            <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
