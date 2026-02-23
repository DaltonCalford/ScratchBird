#coding:utf-8

"""
ID:          issue-3204
ISSUE:       3204
TITLE:       If stored procedure or trigger contains query with PLAN ORDER it could fail
  after disconnect of attachment where procedure/trigger executed first time
DESCRIPTION:
JIRA:        CORE-2817
FBTEST:      bugs.core_2817
"""

import pytest
from firebird.qa import *

init_script = """
    create sequence g;
    create or alter procedure sp_test returns(cnt int) as begin end;
    recreate table test(
        x int,
        s varchar(1000)
    );
    commit;

    insert into test(x, s)
    select gen_id(g,1), rpad('', 1000, uuid_to_char(gen_uuid()))
    from rdb$types, rdb$types
    rows 1000;
    commit;

    create index test_x on test(x);
    create index test_s on test(s);
    commit;

    set term ^;
    create or alter procedure sp_test(a_odd smallint) as
        declare c_ord cursor for (
            select s
            from test
            where mod(x, 2) = :a_odd
            order by x
        );
        declare v_s type of column test.s;
    begin
        open c_ord;
        while (1=1) do
        begin
            fetch c_ord into v_s;
            if (row_count = 0) then leave;
            update test set s = uuid_to_char(gen_uuid()) where current of c_ord;
        end
        close c_ord;
    end
    ^ -- sp_test
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=2.5')
def test_1(act: Action):
    with act.db.connect() as att2:
        with act.db.connect() as att1:
            cur1 = att1.cursor()
            cur2 = att2.cursor()
            cur1.execute('execute procedure sp_test(0)')
            cur2.execute('execute procedure sp_test(1)')
            att1.commit()
        cur2.execute('execute procedure sp_test(1)')
