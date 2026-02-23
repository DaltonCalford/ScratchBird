#coding:utf-8

"""
ID:          issue-1919
ISSUE:       1919
TITLE:       Make INSERTING/UPDATING/DELETING reserved words to fix ambiguity with boolean expresions
DESCRIPTION:
JIRA:        CORE-4791
FBTEST:      bugs.core_4791
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test1(id int, "inserting" boolean);
    recreate table test2(id int, "inserting" boolean, "updating" boolean);
    recreate table test3(id int, "inserting" boolean, "updating" boolean, "deleting" boolean);
    recreate table test4(id int, "inserting" boolean, "updating" boolean, "deleting" boolean);
    commit;

    set term ^;
    create or alter trigger trg_test1_bi for test1 active before insert as
    begin
        new."inserting" = true;
    end
    ^

    create or alter trigger trg_test2_bu for test2 active before insert or update as
    begin
        new."inserting" = inserting;
        new."updating" = updating;
    end
    ^

    create or alter trigger trg_test3_bu for test3 active before insert or update or delete as
    begin
        if (deleting) then
            insert into test4(id, "inserting", "updating", "deleting")
            values(old.id, old."inserting", old."updating", deleting);
        else
            begin
                new."inserting" = inserting;
                new."updating" = updating;
            end
    end
    ^
    set term ;^
    commit;

    insert into test1(id) values(1);

    insert into test2(id) values(2);
    update test2 set id=-id;

    insert into test3(id) values(3);

    delete from test3;

    set count on;
    set echo on;
    select * from test1;

    select * from test2;

    select * from test3;

    select * from test4;

    commit;

    recreate table testa(inserting int);
    recreate table testb(updating int);
    recreate table testc(deleting int);
"""

act = isql_act('db', test_script, substitutions=[('line [0-9]+, column [0-9]+', '')])

expected_stdout = """
    select * from test1;

              ID inserting
    ============ =========
               1 <true>

    Records affected: 1

    select * from test2;

              ID inserting updating
    ============ ========= ========
              -2 <false>   <true>

    Records affected: 1

    select * from test3;
    Records affected: 0

    select * from test4;

              ID inserting updating deleting
    ============ ========= ======== ========
               3 <true>    <false>  <true>

    Records affected: 1

    commit;

    recreate table testa(inserting int);
    recreate table testb(updating int);
    recreate table testc(deleting int);
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 20
    -inserting

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 20
    -updating

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 20
    -deleting
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

