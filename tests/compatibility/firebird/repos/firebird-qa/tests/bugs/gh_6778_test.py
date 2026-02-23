#coding:utf-8

"""
ID:          issue-6778
ISSUE:       6778
TITLE:       Inconsistent cursor-driven deletion
DESCRIPTION:
    https://github.com/FirebirdSQL/firebird/issues/6778
    
    Confirmed bug on: WI-V4.0.0.2436.
    Checked on: 4.0.0.2448 - works fine.
    No errors must be during execution of this code.
FBTEST:      bugs.gh_6778
NOTES:
    [04.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.894; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

fb4_test_script = """
    create table a1 (id int);
    create table a2 (id int);

    create index ia1 on a1 (id);
    create index ia2 on a2 (id);

    commit;

    insert into a1 values (1);
    insert into a2 values (1);
    insert into a2 values (1);

    commit;

    set plan;

    set term ^;
    execute block
    as
    begin
      for select a1.id from a1, a2 where a2.id = a1.id+0 as cursor c do
        delete from a1 where current of c;
    end^
    set term ;^

    -- PLAN JOIN (C A1 NATURAL, C A2 INDEX (IA2))
    -- Statement failed, SQLSTATE = 22000
    -- no current record for fetch operation
    -- -At block line: 5, col: 5

    rollback;

    set term ^;
    execute block
    as
    begin
      for select a1.id from a1, a2 where a2.id+0 = a1.id as cursor c do
        delete from a1 where current of c;
    end^
    set term ;^

    -- PLAN JOIN (C A2 NATURAL, C A1 INDEX (IA1))

    rollback;
"""

fb4_expected_stdout = """
    PLAN JOIN (C A1 NATURAL, C A2 INDEX (IA2))
    PLAN JOIN (C A2 NATURAL, C A1 INDEX (IA1))
"""

fb5_test_script = """
    create table a1 (id int);
    create table a2 (id int);

    create index ia1 on a1 (id);
    create index ia2 on a2 (id);

    commit;

    insert into a1 values (1);
    insert into a2 values (1);
    insert into a2 values (1);

    commit;

    set plan;

    set term ^;
    execute block
    as
    begin
        for
            select a1.id from a1, a2 where a2.id = a1.id
            PLAN JOIN (A1 NATURAL, A2 INDEX (IA2))
            as cursor c
        do
            delete from a1 where current of c;
    end
    ^
    rollback
    ^

    execute block
    as
    begin
        for
            select a1.id from a1, a2 where a2.id = a1.id
            PLAN JOIN (A2 NATURAL, A1 INDEX (IA1))
            as cursor c
        do
            delete from a1 where current of c;
    end
    ^
    set term ;^
"""

fb5_expected_stdout = """
    -- line 4, column 9
    PLAN JOIN (C A1 NATURAL, C A2 INDEX (IA2))
    -- line 4, column 9
    PLAN JOIN (C A2 NATURAL, C A1 INDEX (IA1))
"""

fb6_expected_stdout = """
    -- line, column
    PLAN JOIN ("C" "PUBLIC"."A1" NATURAL, "C" "PUBLIC"."A2" INDEX ("PUBLIC"."IA2"))
    -- line, column
    PLAN JOIN ("C" "PUBLIC"."A2" NATURAL, "C" "PUBLIC"."A1" INDEX ("PUBLIC"."IA1"))
"""

act = python_act('db', substitutions=[('-- line(:)?\\s+\\d+,\\s+col(umn)?(:)?\\s+\\d+', '-- line, column')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    test_script = fb5_test_script if act.is_version('>=5') else fb4_test_script
    expected_stdout = fb4_expected_stdout if act.is_version('<5') else fb5_expected_stdout if act.is_version('<6') else fb6_expected_stdout
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input=test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
