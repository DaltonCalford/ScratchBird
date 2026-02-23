#coding:utf-8

"""
ID:          issue-3213
ISSUE:       3213
TITLE:       Join condition fails for UTF-8 databases
DESCRIPTION:
JIRA:        CORE-2826
FBTEST:      bugs.core_2826
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    commit;

    create collation unicode_nopad for utf8 from unicode no pad;
    -- !!! >>> COMMENTED THIS ACCORDING TO NOTES IN THE TICKET: >>> commit; <<<
    -- Table is created in the same transaction as collation:
    create table tst1_nopad (
      k1 varchar(3) character set utf8 collate unicode_nopad,
      k2 int,
      k3 char(1)  character set utf8 collate unicode_nopad,
      primary key (k1, k2, k3) using index txt1_nopad_pk
    );
    commit;

    insert into tst1_nopad values ('ap', 123, ' ');
    insert into tst1_nopad values ('hel', 666, 'v');
    commit;

    set list on;
    set plan on;
    select t1.*
      from tst1_nopad t1
     where t1.k1 = 'ap'
       and t1.k2 = 123
       and t1.k3 = ' '
    plan (t1 natural);

    select t1.*
      from tst1_nopad t1
     where t1.k1 = 'ap'
       and t1.k2 = 123
       and t1.k3 = ' ';

     -- 'show table' was removed, see CORE-4782 ("Command `SHOW TABLE` fails..." - reproduced on Windows builds 2.5 and 3.0 only)
"""

substitutions = [('[ \t]+', ' '), ('-At line.*', '')]

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (T1 NATURAL)
    K1 ap
    K2 123
    K3
    PLAN (T1 INDEX (TXT1_NOPAD_PK))
    K1 ap
    K2 123
    K3
"""

expected_stdout_6x = """
    PLAN ("T1" NATURAL)
    K1 ap
    K2 123
    K3
    PLAN ("T1" INDEX ("PUBLIC"."TXT1_NOPAD_PK"))
    K1 ap
    K2 123
    K3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
