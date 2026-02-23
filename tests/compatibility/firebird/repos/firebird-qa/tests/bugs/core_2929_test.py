#coding:utf-8

"""
ID:          issue-3312
ISSUE:       3312
TITLE:       "Invalid ESCAPE sequence" when connecting to the database
DESCRIPTION:
JIRA:        CORE-2929
FBTEST:      bugs.core_2929
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;
    set list on;

    recreate table t(id int, who_am_i varchar(31) default current_user, whats_my_role varchar(31) default current_role);
    commit;
    insert into t(id) values(0);
    commit;

    create user "#" password '#';
    create role "##";
    commit;

    grant "##" to "#";
    commit;

    grant select, insert, update, delete on t to role "##";
    commit;

    connect '$(DSN)' user "#" password '#' role "##";
    insert into t(id) values(1);
    insert into t(id) values(2);
    update t set id = -id where id = 1;
    delete from t where id = 0;

    select * from t order by id;
    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey';
    drop role "##";
    drop user "#";
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              -1
    WHO_AM_I                        #
    WHATS_MY_ROLE                   ##

    ID                              2
    WHO_AM_I                        #
    WHATS_MY_ROLE                   ##
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

