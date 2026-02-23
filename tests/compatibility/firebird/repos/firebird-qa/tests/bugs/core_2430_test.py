#coding:utf-8

"""
ID:          issue-2846
ISSUE:       2846
TITLE:       Server adds "NOT" at the end of default value for the TIMESTAMP field
DESCRIPTION:
JIRA:        CORE-2430
FBTEST:      bugs.core_2430
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table t1 (
        f1 bigint not null,
        f2 bigint not null,
        f3 timestamp default current_timestamp not null
    );

    alter table t1 add constraint pk_t1 primary key (f1, f2);

    show table t1;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    F1                              BIGINT Not Null
    F2                              BIGINT Not Null
    F3                              TIMESTAMP Not Null default current_timestamp
    CONSTRAINT PK_T1:
    Primary key (F1, F2)
"""

expected_stdout_6x = """
    Table: PUBLIC.T1
    F1                              BIGINT Not Null
    F2                              BIGINT Not Null
    F3                              TIMESTAMP Not Null default current_timestamp
    CONSTRAINT PK_T1:
    Primary key (F1, F2)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
