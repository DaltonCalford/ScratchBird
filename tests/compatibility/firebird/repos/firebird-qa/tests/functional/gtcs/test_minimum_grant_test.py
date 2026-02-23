#coding:utf-8

"""
ID:          gtcs.minimum-grant
TITLE:       Minimum grant test
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_34.script
FBTEST:      functional.gtcs.minimum_grant_test
"""

import pytest
from firebird.qa import *

substitutions = [('no permission for (read/select|SELECT) access.*', 'no permission for read access'),
                 ('no permission for (insert/write|INSERT) access.*', 'no permission for write access'),
                 ('[ \t]+', ' '), ('-{0,1}[ ]{0,1}Effective user is.*', '')]

db = db_factory()

tmp_user1 = user_factory('db', name='tmp_gtcs_34_a', password='123')
tmp_user2 = user_factory('db', name='tmp_gtcs_34_b', password='456')

act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action, tmp_user1: User, tmp_user2: User):

    test_sql = f"""
        set list on;

        create table test (f01 int);
        commit;

        grant insert on test to {tmp_user1.name};
        grant select on test to {tmp_user2.name};
        commit;

        -------------------------------------------------
        connect '{act.db.dsn}' user {tmp_user1.name} password '{tmp_user1.password}';
        select current_user as whoami from rdb$database;
        insert into test values(1); -- should pass
        select * from test; -- should fail
        commit;

        -------------------------------------------------
        connect '{act.db.dsn}' user {tmp_user2.name} password '{tmp_user2.password}';
        select current_user as whoami from rdb$database;
        insert into test values(2); -- should fail
        select * from test; -- should pass
        commit;
    """

    act.expected_stdout = f"""
        WHOAMI {tmp_user1.name.upper()}
        
        Statement failed, SQLSTATE = 28000
        no permission for read access
        
        WHOAMI {tmp_user2.name.upper()}
        
        Statement failed, SQLSTATE = 28000
        no permission for write access
        
        F01 1
    """
    act.isql(input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
