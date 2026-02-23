#coding:utf-8

"""
ID:          issue-8278
ISSUE:       8278
TITLE:       Avoid index lookup for a NULL key if the condition is known to always be FALSE in this case
DESCRIPTION: Test uses ticket example but with reduced number of records that will be inserted into tables.
             Number of fetches is compared with MAX_FETCHES_ALERT. After fix trace shows that it is ~5600.
NOTES:
    [24.01.2025] pzotov
    Commits that fixed problem:
        6.x: https://github.com/FirebirdSQL/firebird/commit/58633c81ea490326d880f42780bb7f293c2a0ae8
        5.x: https://github.com/FirebirdSQL/firebird/commit/22d23c17d94e390f4ca058afff6ac0338d014225

    Confirmed problem on 6.0.0.647-9fccb55.
    Checked on intermediate snapshots: 6.0.0.652-58633c8; 5.0.3.1622-22d23c1
"""

from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

T1_COUNT = 30000
T2_COUNT = 100
T3_COUNT = 10
MAX_FETCHES_ALERT = 6000

init_sql = f"""
    recreate table t1 (t1_id int not null);
    recreate table t2 (t2_id int not null, t1_id int);
    recreate table t3 (t3_id int not null, t1_id int);

    set term ^;
    execute block as
        declare l_t1_id int;
        declare l_t2_id int;
        declare l_t3_id int;
        declare n1 int = {T1_COUNT};
        declare n2 int = {T2_COUNT};
        declare n3 int = {T3_COUNT};
    begin
        l_t3_id = 1;
        while (l_t3_id <= n1) do
        begin
            l_t1_id = iif(mod(l_t3_id, n2) = 0, trunc(l_t3_id/n2), null);
            l_t2_id = iif(mod(l_t3_id, n3) = 0, trunc(l_t3_id/n3), null);

            if (l_t1_id is not null) then
                insert into t1 (t1_id) values (:l_t1_id);
        
            insert into t3 (t3_id, t1_id) values (:l_t3_id, :l_t1_id);

            if (l_t2_id is not null) then
                insert into t2 (t2_id, t1_id) values (:l_t2_id, :l_t1_id);

            l_t3_id = l_t3_id + 1;
        end
    end
    ^
    set term ;^
    commit;

    alter table t1 add constraint t1_pk primary key (t1_id);
    alter table t2 add constraint t2_pk primary key (t2_id);
    alter table t3 add constraint t3_pk primary key (t3_id);
    alter table t2 add constraint t2_fk foreign key (t1_id) references t1 (t1_id);
    alter table t3 add constraint t3_fk foreign key (t1_id) references t1 (t1_id);
    commit;
"""
db = db_factory(init = init_sql)
act = python_act('db')

@pytest.mark.version('>=5.0.3')
def test_1(act: Action, capsys):

    expected_msg = 'Number of fetches: EXPECTED'
    with act.db.connect() as con:
        cur = con.cursor()
        test_sql = """
            select count(*) /* trace_me */
            from t2
            left outer join t1 on t1.t1_id = t2.t1_id
            left outer join t3 on t3.t1_id = t1.t1_id
        """

        fetches_ini = con.info.fetches
        cur.execute(test_sql)
        cur.fetchall()
        sql_fetches = con.info.fetches - fetches_ini
        print(expected_msg if sql_fetches <= MAX_FETCHES_ALERT else f'Number of fetches UNEXPECTED: {sql_fetches} - greater than {MAX_FETCHES_ALERT}' )

    act.expected_stdout = f"""
        {expected_msg}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
