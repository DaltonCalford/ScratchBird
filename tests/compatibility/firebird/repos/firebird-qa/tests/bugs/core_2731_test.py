#coding:utf-8

"""
ID:          issue-3126
ISSUE:       3126
TITLE:       Recursive EXECUTE STATEMENT works wrong
DESCRIPTION:
JIRA:        CORE-2731
FBTEST:      bugs.core_2731
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table SQL_SOURCE(
        SQL_SOURCE varchar(32000)
    );
    commit;

    insert into SQL_SOURCE values(
        'execute block as
        declare variable SQL type of column SQL_SOURCE.SQL_SOURCE;
        begin
            select first(1) SQL_SOURCE from SQL_SOURCE into :SQL;
            execute statement :SQL
            -- YOUR DB
            on external ''$(DSN)''
            as user ''SYSDBA'' password ''masterkey'';
        end'
    );
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set term ^ ;
    execute block as
        declare v_sql type of column SQL_SOURCE.SQL_SOURCE;
    begin
        select first(1) SQL_SOURCE from SQL_SOURCE into :v_sql;
        execute statement :v_sql;
    end ^
    set term ; ^
    rollback;
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

act = isql_act('db', test_script, substitutions=[('[-]{0,1}At block line: [\\d]+, col: [\\d]+', '')])

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement error at isc_dsql_execute2 :
    335544926 : Execute statement...
    -At block line: 5, col: 5
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

