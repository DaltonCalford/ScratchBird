#coding:utf-8

"""
ID:          issue-7691
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7691
TITLE:       'with caller privileges' has no effect in triggers
DESCRIPTION:
NOTES:
    [07.09.2023]
    Confirmed bug on 5.0.0.1182.
    Checked on 5.0.0.1190, 4.0.4.2986.
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp_user_7691', password='123')

act = python_act('db')

@pytest.mark.version('>=4.0.3')
def test_1(act: Action, tmp_user: User, capsys):
    test_sql = f"""
        set list on;
        set bail on;
        create table test( 
            id   int
           ,name  varchar(15)
        );
        commit;
        insert into test(id , name) values (1, 'qwerty');
        commit;

        create table test_2(
            id   int
           ,name  varchar(15)
        );
        grant all on table test_2 to user {tmp_user.name};
        commit;

        set term ^;
        create or alter trigger test_2_trigger  
            for      test_2
            active   before insert or update or delete 
            position 100 
        as
           declare k int;
        begin     
            execute statement 'select first 1 1 from test '
                with caller privileges
                into k;
        end
        ^
        set term ;^

        grant all on table test_2 to trigger test_2_trigger;

        grant select on table test to trigger test_2_trigger; -- <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
        commit;
        
        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}';
        insert into test_2(id, name) values (1234, 'qwerty');
        commit;

        select * from test_2;
    """

    expected_stdout = """
        ID                              1234
        NAME                            qwerty
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
