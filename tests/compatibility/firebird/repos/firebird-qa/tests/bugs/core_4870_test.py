#coding:utf-8

"""
ID:          issue-5166
ISSUE:       5166
TITLE:       Incorrect number of affected rows for UPDATE against VIEW created WITH CHECK OPTION
DESCRIPTION:
JIRA:        CORE-4870
FBTEST:      bugs.core_4870
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Checked on LI-V3.0.0.32008, build after commiting rev. http://sourceforge.net/p/firebird/code/62140
    create or alter view v_test3 as select 1 id from rdb$database;
    create or alter view v_test2 as select 1 id from rdb$database;
    create or alter view v_test1 as select 1 id from rdb$database;
    commit;

    recreate table test(id int primary key, x int);
    commit;
    insert into test values(1, 100);
    insert into test values(2, 200);
    insert into test values(3, 300);
    insert into test values(4, 400);
    insert into test values(5, 500);
    insert into test values(6, 600);
    commit;

    recreate view v_test1 as select * from test where mod(id, 3)=0;
    recreate view v_test2 as select * from test where mod(id, 3)=0 with check option;
    recreate view v_test3 as select * from v_test2 where mod(id, 3)=0 with check option;
    commit;

    set count on;
    update v_test1 set x = -x where mod(id,3) = 0;
    rollback;

    update v_test2 set x = -x where mod(id,3) = 0;
    rollback;

    update v_test3 set x = -x where mod(id,3) = 0;
    rollback;

    merge into v_test1 t using( select id, x from test ) s on t.id = s.id
    when matched then update set x = -s.x
    when not matched then insert(id, x) values( -3 * s.id, -s.x )
    ;
    rollback;

    merge into v_test2 t using( select id, x from test ) s on t.id = s.id
    when matched then update set x = -s.x
    when not matched then insert(id, x) values( -3 * s.id, -s.x )
    ;
    rollback;

    merge into v_test3 t using( select id, x from test ) s on t.id = s.id
    when matched then update set x = -s.x
    when not matched then insert(id, x) values( -3 * s.id, -s.x )
    ;
    rollback;

    insert into v_test1(id, x) values( 9,  900);
    insert into v_test2(id, x) values(12, 1200);
    insert into v_test3(id, x) values(15, 1500);
    rollback;

    set count off;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 2
    Records affected: 2
    Records affected: 2
    Records affected: 6
    Records affected: 6
    Records affected: 6
    Records affected: 1
    Records affected: 1
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

