#coding:utf-8

"""
ID:          issue-4729
ISSUE:       4729
TITLE:       Grant and Revoke update (field) [CORE4407]
NOTES:
    [16.02.2023] pzotov
    Confirmed bug on 5.0.0.843, 4.0.3.2876, 3.0.11.33639 - got:
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to COLUMN TEST.AGE
    Checked on 5.0.0.938, 4.0.3.2900, 3.0.11.33664 -- all fine
    ::: NOTE :::
    If script is executed from command line using ISQL then problem looks as described in the ticket.
    But if the same script is executed from firebird-qa then FB 5.0.0.843 crashes.

"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user_1 = user_factory('db', name='tmp_gh_4729_foo', password='123')
tmp_user_2 = user_factory('db', name='tmp_gh_4729_bar', password='456')

act = python_act('db')

expected_stdout = """
    AGE                             1
    WHO                             TMP_GH_4729_FOO
    AGE                             2
    WHO                             TMP_GH_4729_BAR
    AGE                             3
    WHO                             TMP_GH_4729_FOO
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user_1: User, tmp_user_2: User):

    test_script = f"""
        set list on;
        create table test (
            id int
            ,name varchar(10)
            ,age integer
            ,who varchar(31)
            ,constraint test_pk primary key (id)
        );

        commit;

        insert into test(id, name, age) values (1, 'TEST', 99);
        commit;

        grant select, update(AGE), update(who) on test to {tmp_user_1.name}, {tmp_user_2.name};
        commit;

        connect '{act.db.dsn}' user {tmp_user_1.name} password '{tmp_user_1.password}';
        update test set AGE = 1, who = current_user returning age, who;
        commit;

        connect '{act.db.dsn}' user {tmp_user_2.name} password '{tmp_user_2.password}';
        update test set AGE = 2, who = current_user returning age, who;
        commit;

        -- connect as sysdba to revoke grant from user-1:
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        revoke select, update(AGE) on test from {tmp_user_2.name};
        commit;

        connect '{act.db.dsn}' user {tmp_user_1.name} password '{tmp_user_1.password}';
        update test set AGE = 3, who = current_user returning age, who;
        commit;

    """
    
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
