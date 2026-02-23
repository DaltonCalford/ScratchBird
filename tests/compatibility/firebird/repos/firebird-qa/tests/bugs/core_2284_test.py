#coding:utf-8

"""
ID:          issue-2710-5943
ISSUE:       2710, 5943
TITLE:       Records left in RDB$PAGES after rollback of CREATE TABLE statement
DESCRIPTION:
JIRA:        CORE-2284
FBTEST:      bugs.core_2284
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    recreate table test_cs__master (
        str_pk varchar(32) character set UTF8 not null,
        primary key (str_pk) using index test_s_master_pk
    );

    recreate table test_cs__detail (
        str_pk varchar(32) character set WIN1251 not null,
        foreign key (str_pk) references test_cs__master (str_pk)
    );

    commit; -- this will raise: "SQLSTATE = 42000 / -partner index segment no 1 has incompatible data type"

    rollback;

    set list on;

    select count(*)
    from rdb$pages
    where rdb$relation_id >= (select rdb$relation_id from rdb$database);

    rollback;
"""

act = isql_act('db', test_script)

expected_stdout = """
    COUNT                           0
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -partner index segment no 1 has incompatible data type
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

