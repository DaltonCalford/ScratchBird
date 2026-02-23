#coding:utf-8

"""
ID:          issue-6234
ISSUE:       6234
TITLE:       error read permission for BLOB field, when it is input/output procedure`s parametr
DESCRIPTION:
JIRA:        CORE-5982
FBTEST:      bugs.core_5982
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name='tmp$c5982', password='123')

test_script = """
    set bail on;
    recreate table my_table (
       my_num integer
      ,my_data blob
    );
    commit;

    insert into my_table(my_num , my_data) values (1, 'qwerty');
    commit;

    set term ^;
    create or alter procedure sp_worker(my_data blob) as
        declare variable my_value blob;
    begin
        my_value = my_data ;
        rdb$set_context('USER_SESSION', 'SP_WORKER', 'DONE BY ' || current_user );
    end
    ^

    create or alter procedure sp_main as
        declare variable my_data blob;
    begin
        select my_data
          from my_table
         where my_num = 1
         into: my_data;

         execute procedure sp_worker(my_data);

    end
    ^
    set term ;^
    commit;

    grant select on table my_table to procedure sp_main;
    grant execute on procedure sp_worker to procedure sp_main;
    grant execute on procedure sp_main to public;
    commit;

    set list on;

    connect '$(DSN)' user 'tmp$c5982' password '123';

    execute procedure sp_main;
    select rdb$get_context('USER_SESSION', 'SP_WORKER') as result from rdb$database;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT                          DONE BY TMP$C5982
"""

@pytest.mark.version('>=3')
def test_1(act: Action, tmp_user):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
