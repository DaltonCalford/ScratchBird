#coding:utf-8

"""
ID:          issue-2240
ISSUE:       2240
TITLE:       Usernames with '.' character
DESCRIPTION:
JIRA:        CORE-1810
FBTEST:      bugs.core_1810
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set wng off;
    recreate table test(id int, x int, y int, z int);
    commit;
    insert into test values(1, 100, 200, 300);
    commit;
    drop user "$.$";
    commit;
    create role "#.#";
    commit;
    create user "$.$" password '123';
    commit;

    revoke all on all from "$.$";
    grant "#.#" to "$.$";
    grant select on test to "#.#";
    commit;

    connect '$(DSN)' user "$.$" password '123' role "#.#";
    commit;

    select
        current_user,
        current_role,
        iif( upper(a.mon$remote_protocol) starting with upper('TCP'), 'YES', 'NO!') is_remote_connection
    from rdb$database m
    join mon$attachments a on a.mon$attachment_id = current_connection
    ;

    select * from test;
    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey';
    commit;
    drop user "$.$";
    commit;
"""

act = isql_act('db', test_script,
                 substitutions=[('Statement failed, SQLSTATE.*', ''), ('record not found for user:.*', '')])

expected_stdout = """
    USER                            $.$
    ROLE                            #.#
    IS_REMOTE_CONNECTION            YES

    ID                              1
    X                               100
    Y                               200
    Z                               300
"""

expected_stderr = """
    Statement failed, SQLSTATE = HY000
    record not found for user: $.$
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)

