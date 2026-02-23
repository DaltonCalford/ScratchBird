#coding:utf-8

"""
ID:          issue-6240
ISSUE:       6240
TITLE:       Pool of external connections
DESCRIPTION:
  Test assumes that firebird.conf contains:
    ExtConnPoolSize = 100 (or at any other value >= 6)
    ExtConnPoolLifeTime = 10
  We run six execute blocks with COMMIT after each of them.
  When EDS pool is enabled then every new execute block will use the same attachment as it was established in the 1st EB.
  We check this by running query that show number of duplicates for each of N attachments: this number must be equal to N-1.
  ::: NB :::
  Final statement must be 'ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL' otherwise DB file will be kept by engine at least
  for 10 seconds after this test finish (see parameter 'ExtConnPoolLifeTime').

  Thank hvlad for additional explanations, discuss in e-mail was 26.04.19 09:38.
JIRA:        CORE-5990
FBTEST:      bugs.core_5990
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v_conn as
    select
        cast(rdb$get_context('SYSTEM', 'EXT_CONN_POOL_SIZE') as int) as pool_size,
        cast(rdb$get_context('SYSTEM', 'EXT_CONN_POOL_IDLE_COUNT') as int) as pool_idle,
        cast(rdb$get_context('SYSTEM', 'EXT_CONN_POOL_ACTIVE_COUNT') as int) as pool_active,
        cast(rdb$get_context('SYSTEM', 'EXT_CONN_POOL_LIFETIME') as int) as pool_lifetime
    from rdb$database
    ;

    create sequence g;
    commit;

    recreate table att_info(id int, established_attach_id int);
    commit;

    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'DBA_USER', 'SYSDBA');
        rdb$set_context('USER_SESSION', 'DBA_PSWD', 'masterkey');
    end
    ^

    execute block as
        declare con1 int;
    begin
        execute statement 'select current_connection from rdb$database'
            on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
            as user rdb$get_context('USER_SESSION', 'DBA_USER') password rdb$get_context('USER_SESSION', 'DBA_PSWD')
        into con1;
        insert into att_info(id, established_attach_id) values( gen_id(g,1), :con1 );

    end
    ^
    commit -- <<< NOTA BENE <<<<  C.O.M.M.I.T after each execute block <<<
    ^

    execute block as
        declare con1 int;
    begin
        execute statement 'select current_connection from rdb$database'
            on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
            as user rdb$get_context('USER_SESSION', 'DBA_USER') password rdb$get_context('USER_SESSION', 'DBA_PSWD')
        into con1;
        insert into att_info(id, established_attach_id) values( gen_id(g,1), :con1 );

    end
    ^
    commit
    ^

    execute block as
        declare con1 int;
    begin
        execute statement 'select current_connection from rdb$database'
            on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
            as user rdb$get_context('USER_SESSION', 'DBA_USER') password rdb$get_context('USER_SESSION', 'DBA_PSWD')
        into con1;
        insert into att_info(id, established_attach_id) values( gen_id(g,1), :con1 );

    end
    ^
    commit
    ^

    execute block as
        declare con1 int;
    begin
        execute statement 'select current_connection from rdb$database'
            on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
            as user rdb$get_context('USER_SESSION', 'DBA_USER') password rdb$get_context('USER_SESSION', 'DBA_PSWD')
        into con1;
        insert into att_info(id, established_attach_id) values( gen_id(g,1), :con1 );

    end
    ^
    commit
    ^

    execute block as
        declare con1 int;
    begin
        execute statement 'select current_connection from rdb$database'
            on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
            as user rdb$get_context('USER_SESSION', 'DBA_USER') password rdb$get_context('USER_SESSION', 'DBA_PSWD')
        into con1;
        insert into att_info(id, established_attach_id) values( gen_id(g,1), :con1 );

    end
    ^
    commit
    ^

    execute block as
        declare con1 int;
    begin
        execute statement 'select current_connection from rdb$database'
            on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
            as user rdb$get_context('USER_SESSION', 'DBA_USER') password rdb$get_context('USER_SESSION', 'DBA_PSWD')
        into con1;
        insert into att_info(id, established_attach_id) values( gen_id(g,1), :con1 );

    end
    ^
    commit
    ^
    set term ;^

    --set echo on;
    --select * from v_conn;
    --select a.id, a.established_attach_id, count(*)over(partition by established_attach_id)-1 dup_cnt

    set list on;
    select a.id, count(*)over(partition by established_attach_id)-1 dup_cnt
    from att_info a
    order by id;

    ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL; -- !! mandatory otherwise database file will be kept by engine and fbtest will not able to drop it !!
    commit;

"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    DUP_CNT                         5
    ID                              2
    DUP_CNT                         5
    ID                              3
    DUP_CNT                         5
    ID                              4
    DUP_CNT                         5
    ID                              5
    DUP_CNT                         5
    ID                              6
    DUP_CNT                         5
"""

@pytest.mark.es_eds
@pytest.mark.version('>=4.0')
def test_1(act: Action):
    if int(act.get_config('ExtConnPoolSize')) < 6 or int(act.get_config('ExtConnPoolLifeTime')) < 10:
        pytest.skip('Needs config: ExtConnPoolSize >=6, ExtConnPoolLifeTime >= 10')
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
