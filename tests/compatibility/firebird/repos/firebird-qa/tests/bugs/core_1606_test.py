#coding:utf-8

"""
ID:          issue-2027
ISSUE:       2027
TITLE:       Ability to insert child record if parent record is locked but foreign key target unchanged
DESCRIPTION:
  Master table has two record, both are updated but without changing PK field.
  Than we check that we CAN add rows in detail table with references to existed PK from main,
  and even can change FK-values in these added rows, but can do it only with maintenance that
  MAIN table's PK exists for new values in FK
JIRA:        CORE-1606
FBTEST:      bugs.core_1606
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
    begin
        execute statement 'drop sequence g';
        when any do begin end
    end
    ^
    set term ;^
    commit;
    create sequence g;
    commit;
    recreate table tdetl(id int constraint tdetl_pk primary key, pid int);
    commit;
    recreate table tmain(id int constraint tmain_pk primary key, x int);
    commit;
    alter table tdetl add constraint tdetl_fk foreign key(pid) references tmain(id);
    commit;
    insert into tmain(id, x) values(1, 100);
    insert into tmain(id, x) values(2, 200);
    commit;

    set list on;
    set transaction no wait;

    --set count on;
    update tmain set id = id, x = -x*10 where id=1;
    update tmain set id = id, x = -x*20 where id=2;

    set term ^;
    execute block returns(id int, pid int) as
       declare s varchar(1024);
    begin

        s = 'insert into tdetl(id, pid) select gen_id(g,1), cast( ? as int) from rdb$types rows 3';
        /*
        -- todo later, after fix CORE-4796
        'merge into tdetl t '
        || 'using ( select 5 as id, 1 as pid from rdb$types rows 2 ) s '
        || 'on t.id=s.id '
        || 'when matched then update set t.pid = s.pid '
        || 'when not matched then insert values( s.id, s.pid)';
        */
        execute statement ( s  ) ( 1 ) -------------------------------------- [1]: add rows with pid=1
        on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
        as user 'sysdba' password 'masterkey' role 'RCHILD';

        execute statement ( s  ) ( 2 ) -------------------------------------- [2]: add rows with pid=2
        on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
        as user 'sysdba' password 'masterkey' role 'RCHILD';

        execute statement ('update tdetl set pid = 3 - pid') -- rows with pid=1 that were inserted on [1] will have pid=2 and vice versa
        on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
        as user 'sysdba' password 'masterkey' role 'RCHILD';

        for
           execute statement 'select id,pid from tdetl order by id'
           on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
           as user 'sysdba' password 'masterkey' role 'RCHILD'
           into id,pid
        do
           suspend;

    end
    ^
    set term ;^
    commit;

    --                                    ||||||||||||||||||||||||||||
    -- ###################################|||  FB 4.0+, SS and SC  |||##############################
    --                                    ||||||||||||||||||||||||||||
    -- If we check SS or SC and ExtConnPoolLifeTime > 0 (config parameter FB 4.0+) then current
    -- DB (bugs.core_NNNN.fdb) will be 'captured' by firebird.exe process and fbt_run utility
    -- will not able to drop this database at the final point of test.
    -- Moreover, DB file will be hold until all activity in firebird.exe completed and AFTER this
    -- we have to wait for <ExtConnPoolLifeTime> seconds after it (discussion and small test see
    -- in the letter to hvlad and dimitr 13.10.2019 11:10).
    -- This means that one need to kill all connections to prevent from exception on cleanup phase:
    -- SQLCODE: -901 / lock time-out on wait transaction / object <this_test_DB> is in use
    -- #############################################################################################
    delete from mon$attachments where mon$attachment_id != current_connection;
    commit;

"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    PID                             2
    ID                              2
    PID                             2
    ID                              3
    PID                             2
    ID                              4
    PID                             1
    ID                              5
    PID                             1
    ID                              6
    PID                             1
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

