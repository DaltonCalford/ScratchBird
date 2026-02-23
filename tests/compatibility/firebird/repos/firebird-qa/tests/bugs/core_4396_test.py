#coding:utf-8

"""
ID:          issue-4718
ISSUE:       4718
TITLE:       incorrect result query if it is execute through "execute statement"
DESCRIPTION:
JIRA:        CORE-4396
FBTEST:      bugs.core_4396
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(run smallint, rn smallint, id int);
    commit;

    insert into test(run, rn, id)
    select 1, row_number()over(), r.rdb$relation_id
    from rdb$relations r
    order by rdb$relation_id rows 3;
    commit;

    set term ^;
    execute block returns ( id integer ) as
        declare r int;
        declare i int;
    begin
        for
            execute statement
                'select row_number()over(), rdb$relation_id from rdb$relations order by rdb$relation_id rows 3'
            on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
                as user 'sysdba' password 'masterkey'
            into r, i
        do insert into test(run, rn, id) values(2, :r, :i);
    end
    ^
    set term ;^
    commit;

    set list on;
    select count(*) cnt
    from (
        select rn,id --,min(run),max(run)
        from test
        group by 1,2
        having max(run)-min(run)<>1
    ) x;
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
    CNT                             0
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

