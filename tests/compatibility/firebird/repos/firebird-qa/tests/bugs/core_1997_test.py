#coding:utf-8

"""
ID:          issue-2434
ISSUE:       2434
TITLE:       Broken foreign key handling for multi-segmented index using unicode_ci collation
DESCRIPTION:
JIRA:        CORE-1997
FBTEST:      bugs.core_1997
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
    create table pk (
        c1 varchar (5) character set utf8 collate unicode_ci,
        c2 varchar (5) character set utf8 collate unicode_ci,
        primary key (c1, c2)
    );
    create table fk (
        c1 varchar (5) character set utf8 collate unicode_ci,
        c2 varchar (5) character set utf8 collate unicode_ci,
        foreign key (c1, c2) references pk
    );
    insert into pk values ('a', 'b');
    insert into fk values ('A', 'b');
    commit;
    delete from pk; -- should not be allowed
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint "INTEG_2" on table "FK"
    -Foreign key references are present for the record
    -Problematic key value is ("C1" = 'a', "C2" = 'b')
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint "INTEG_2" on table "PUBLIC"."FK"
    -Foreign key references are present for the record
    -Problematic key value is ("C1" = 'a', "C2" = 'b')
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
