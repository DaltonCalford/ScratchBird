#coding:utf-8

"""
ID:          issue-5142
ISSUE:       5142
TITLE:       Altering a trigger indicating other table than the original does not reflect the change
DESCRIPTION:
JIRA:        CORE-4846
FBTEST:      bugs.core_4846
NOTES:
    [30.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()


TRG_BODY_01 = """
    begin
        new.id2 = coalesce( new.id2, gen_id(g, 1) );
    end
"""

TRG_BODY_02 = """
    begin
        new.id1 = coalesce( new.id1, gen_id(g, 1) );
    end
"""

TRG_BODY_03 = """
    begin
        insert into test1(id1, x1) values(old.id2, old.x2);
    end
"""

TRG_BODY_04 = """
    begin
        if (not inserting) then insert into test2(id2, x2) values(old.id1, old.x1);
        else insert into test2(id2, x2) values(new.id1, new.x1);
    end
"""

test_script = f"""
    recreate table test1( id1 int primary key, x1 int);
    recreate table test2( id2 int primary key, x2 int);
    recreate sequence g;
    commit;

    set term ^;
    create or alter trigger muddy_trg for test2 active before insert position 23184 as
    {TRG_BODY_01}
    ^
    set term ;^
    commit;
    show trigger muddy_trg;


    -- Following statements failed on 3.0.0.31907 with:
    -- Statement failed, SQLSTATE = 42S22
    -- invalid request BLR at offset 17
    -- -column ID1 is not defined in table TEST2
    -- Passes OK on LI-V3.0.0.31916 Firebird 3.0 Release Candidate 1.

    set term ^;
    create or alter trigger muddy_trg for test1 inactive before insert position 17895  as
    {TRG_BODY_02}
    ^
    set term ;^
    commit;
    show trigger muddy_trg;

    set term ^;
    recreate trigger muddy_trg for test2 inactive after delete position 11133 as
    {TRG_BODY_03}
    ^
    set term ;^
    commit;
    show trigger muddy_trg;


    set term ^;
    recreate trigger muddy_trg for test1 active before delete or update or insert position 24187 as
    {TRG_BODY_04}
    ^
    set term ;^
    commit;
    show trigger muddy_trg;
"""

act = isql_act('db', test_script, substitutions=[('\\+.*', ''), ('\\=.*', ''),
                                                 ('Trigger text.*', '')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'PUBLIC.'
    expected_stdout = f"""
        Triggers on Table {SQL_SCHEMA_PREFIX}TEST2:
        {SQL_SCHEMA_PREFIX}MUDDY_TRG, Sequence: 23184, Type: BEFORE INSERT, Active
        as
        {TRG_BODY_01}


        Triggers on Table {SQL_SCHEMA_PREFIX}TEST1:
        {SQL_SCHEMA_PREFIX}MUDDY_TRG, Sequence: 17895, Type: BEFORE INSERT, Inactive
        as
        {TRG_BODY_02}

        Triggers on Table {SQL_SCHEMA_PREFIX}TEST2:
        {SQL_SCHEMA_PREFIX}MUDDY_TRG, Sequence: 11133, Type: AFTER DELETE, Inactive
        as
        {TRG_BODY_03}

        Triggers on Table {SQL_SCHEMA_PREFIX}TEST1:
        {SQL_SCHEMA_PREFIX}MUDDY_TRG, Sequence: 24187, Type: BEFORE DELETE OR UPDATE OR INSERT, Active
        as
        {TRG_BODY_04}
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

