#coding:utf-8

"""
ID:          issue-1967-postfix
ISSUE:       1967
TITLE:       Unnecessary index scan happens when the same index is mapped to both WHERE and ORDER BY clauses
DESCRIPTION:
JIRA:        CORE-1550
FBTEST:      bugs.core_1550_postfix
NOTES:
    [23.03.2025] pzotov
        Separated output because plans differ on 6.x vs previous versions since commit fc12c0ef
        ("Unnest IN/ANY/EXISTS subqueries and optimize them using semi-join algorithm (#8061)").
        Checked on 6.0.0.687-730aa8f; 5.0.3.1633-25a0817 
    [16.01.2026] pzotov
        Adjusted output for 6.x: changed plan must be considered as correct
        (letter from dimitr, 15.01.2026 09:12).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- sent to dimitr 30.09.14 at 22:09
    set term ^;
    execute block as
    begin
        execute statement 'drop sequence g';
        when any do begin end
    end^
    set term ;^
    commit;

    create sequence g; commit;
    recreate table td(id int primary key using index td_pk, f01 int, f02 int); commit;
    recreate table tm(id int); commit;

    insert into tm select gen_id(g,1) from rdb$types rows 100;
    commit;

    insert into td(id, f01, f02) select id, (select min(id) from tm), gen_id(g,1) from tm; commit;

    create index td_f01_non_unq on td(f01);
    create unique index td_f01_f02_unq on td(f01, f02); -- ### NB: compound UNIQUE index presens here beside of PK ###
    commit;

    set planonly;

    -- 1. Check for usage when only PK fields are involved:
    select *
    from tm m
    where exists(
        select * from td d where m.id = d.id
        order by d.id --------------------------- ### this "useless" order by should prevent from bitmap creation in 3.0+
    );
    -- Ineffective plan was here:
    -- PLAN (D ORDER TD_PK INDEX (TD_PK))
    -- ...                  ^
    --                      |
    --                      +-----> BITMAP created!

    -- 2. Check for usage when fields from UNIQUE index are involved:
    select *
    from tm m
    where exists(
        select * from td d
        where m.id = d.f01 and d.f02 = 10
        order by d.f01, d.f02 ------------------- ### this "useless" order by should prevent from bitmap creation in 3.0+
    );

    -- Ineffective plan was here:
    -- PLAN (D ORDER TD_F01_F02_UNQ INDEX (TD_F01_F02_UNQ))
    -- ...                           ^
    --                               |
    --                               +-----> BITMAP created!
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    if act.is_version('<6'):
        expected_stdout = """
            PLAN (D ORDER TD_PK)
            PLAN (M NATURAL)

            PLAN (D ORDER TD_F01_F02_UNQ)
            PLAN (M NATURAL)
        """
    else:
        expected_stdout = """
            PLAN HASH ("M" NATURAL, "D" ORDER "PUBLIC"."TD_PK")
            PLAN HASH ("M" NATURAL, SORT ("D" NATURAL))
        """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

